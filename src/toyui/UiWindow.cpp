//  Copyright (c) 2016 Hugo Amiard hugo.amiard@laposte.net
//  This software is provided 'as-is' under the zlib License, see the LICENSE.txt file.
//  This notice and the license may not be removed or altered from any source distribution.

#include <toyui/Config.h>
#include <toyui/UiWindow.h>

#include <toyui/UiLayout.h>

#include <toyui/Widget/RootSheet.h>

#include <toyui/Frame/Frame.h>
#include <toyui/Render/Context.h>
#include <toyui/Render/Renderer.h>

#include <toyui/Controller/Controller.h>

#include <stb_image.h>
#include <dirent.h>

namespace toy
{
	void spritesInFolder(std::vector<object_ptr<Image>>& images, const string& path, const string& subfolder)
	{
		DIR* dir = opendir(path.c_str());
		dirent* ent;

		while((ent = readdir(dir)) != NULL)
			if(ent->d_type & DT_REG)
			{
				string fullpath = path + ent->d_name;
				string name = subfolder + replaceAll(ent->d_name, ".png", "");

				printf("INFO: Adding Image %s\n", fullpath.c_str());

				int width, height, n;
				unsigned char* img = stbi_load(fullpath.c_str(), &width, &height, &n, 4);
				stbi_image_free(img);
				images.emplace_back(make_object<Image>(name, fullpath, width, height));
			}
				

		closedir(dir);
	}

	UiWindow::UiWindow(RenderSystem& system, const string& name, int width, int height, bool fullScreen, User* user)
		: m_renderSystem(system)
		, m_resourcePath(system.m_resourcePath)
		, m_context(system.createContext(name, width, height, fullScreen))
		, m_renderer(system.createRenderer(*m_context))
		, m_renderWindow(*m_context->m_renderWindow)
		, m_images()
		, m_atlas(1024, 1024)
		, m_width(m_renderWindow.m_width)
		, m_height(m_renderWindow.m_height)
		, m_styler(make_object<Styler>(*this))
		, m_rootSheet(nullptr)
		, m_shutdownRequested(false)
		, m_user(user)
	{
		this->init();
	}

	UiWindow::~UiWindow()
	{
		for(auto& image : m_images)
			m_renderer->unloadImage(*image);

		m_rootSheet->store().clear();
	}

	void UiWindow::init()
	{
		printf("INFO: Initializing UiWindow: resource path %s\n", m_resourcePath.c_str());
		m_renderer->setupContext();

		this->initResources();
		this->loadResources();

		Widget::styles().setup(*this);

		m_rootSheet = make_object<RootSheet>(*this);

		m_context->m_inputWindow->initInput(*m_context->m_renderWindow, m_rootSheet->m_mouse, m_rootSheet->m_keyboard);

		this->resize(size_t(m_width), size_t(m_height));
	}

	void UiWindow::initResources()
	{
		string spritePath = m_resourcePath + "interface/uisprites/";

		printf("INFO: Loading Images in path %s\n", spritePath.c_str());

		DIR* dir = opendir(spritePath.c_str());
		dirent* ent;

		spritesInFolder(m_images, spritePath, "");

		while((ent = readdir(dir)) != NULL)
			if(ent->d_type & DT_DIR && string(ent->d_name) != "." && string(ent->d_name) != "..")
				spritesInFolder(m_images, spritePath + ent->d_name + "/", string(ent->d_name) + "/");

		closedir(dir);
	}

	void UiWindow::loadResources()
	{
		m_renderer->loadFont();

		m_atlas.generateAtlas(m_images);

		for(auto& image : m_images)
			m_renderer->loadImage(*image);

		m_renderer->loadImageRGBA(m_atlas.m_image, m_atlas.m_data);
	}

	Image& UiWindow::createImage(const string& name, int width, int height, uint8_t* data, bool filtering)
	{
		m_images.emplace_back(make_object<Image>(name, name, width, height));
		Image& image = *m_images.back();
		image.d_filtering = filtering;
		m_renderer->loadImageRGBA(image, data);
		return image;
	}

	void UiWindow::removeImage(Image& image)
	{
		vector_remove_if(m_images, [&](object_ptr<Image>& current) { return current->d_index == image.d_index; });
		m_renderer->unloadImage(image);
	}

	Image& UiWindow::findImage(const string& name)
	{
		for(auto& image : m_images)
			if(image->d_name == name)
				return *image;
		static Image null; return null;
	}

	void UiWindow::resize(size_t width, size_t height)
	{
		m_width = float(width);
		m_height = float(height);

		m_context->m_inputWindow->resize(width, height);

		m_rootSheet->frame().setSize({ m_width, m_height });
	}

	bool UiWindow::nextFrame()
	{
		if(m_renderWindow.m_width != size_t(m_width)
		|| m_renderWindow.m_height != size_t(m_height))
			this->resize(m_renderWindow.m_width, m_renderWindow.m_height);

		if(m_context->m_renderSystem.m_manualRender)
		{
			m_rootSheet->m_target->render();
			// add sub layers
		}

		bool pursue = !m_shutdownRequested;
		pursue &= m_context->m_renderWindow->nextFrame();
		pursue &= m_context->m_inputWindow->nextFrame();

		size_t tick = m_clock.readTick();
		size_t delta = m_clock.stepTick();

		m_rootSheet->nextFrame(tick, delta);

		return pursue;
	}
	
	void UiWindow::shutdown()
	{
		m_shutdownRequested = true;
	}
}
