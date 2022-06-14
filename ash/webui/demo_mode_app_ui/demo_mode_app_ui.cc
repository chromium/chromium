// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_app_ui.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_page_handler.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "ash/webui/grit/ash_demo_mode_app_resources_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/views/widget/widget.h"

namespace ash {

DemoModeAppUIConfig::DemoModeAppUIConfig(
    base::RepeatingCallback<base::FilePath()> component_path_producer)
    : content::WebUIConfig(content::kChromeUIScheme, kChromeUIDemoModeAppHost),
      component_path_producer_(std::move(component_path_producer)) {}

DemoModeAppUIConfig::~DemoModeAppUIConfig() = default;

std::unique_ptr<content::WebUIController>
DemoModeAppUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<DemoModeAppUI>(web_ui,
                                         component_path_producer_.Run());
}

bool DemoModeAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsDemoModeSWAEnabled();
}

scoped_refptr<base::RefCountedMemory> ReadFile(
    const base::FilePath& absolute_resource_path) {
  std::string data;
  base::ReadFileToString(absolute_resource_path, &data);
  return base::RefCountedString::TakeString(&data);
}

bool ShouldSourceFromComponent(
    const base::flat_set<std::string>& webui_resource_paths,
    const std::string& path) {
  // TODO(b/232945108): Consider changing this logic to check if the absolute
  // path exists in the component. This would still allow us show the default
  // WebUI resource if the requested path isn't found.
  return !webui_resource_paths.contains(path);
}

void DemoModeAppUI::SourceDataFromComponent(
    const base::FilePath& component_path,
    const std::string& resource_path,
    content::WebUIDataSource::GotDataCallback callback) {
  // Convert to GURL to strip out query params and URL fragments
  //
  // TODO (b/234170189): Verify that query params won't be used in the prod Demo
  // App, or add support for them here instead of ignoring them.
  GURL full_url = GURL(kChromeUIDemoModeAppURL + resource_path);
  // Trim leading slash from path
  std::string path = full_url.path().substr(1);

  base::FilePath absolute_resource_path = component_path.AppendASCII(path);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadFile, absolute_resource_path), std::move(callback));
}

DemoModeAppUI::DemoModeAppUI(content::WebUI* web_ui,
                             base::FilePath component_path)
    : ui::MojoWebUIController(web_ui) {
  // We tack the resource path onto this component path, so CHECK that it's
  // absolute so ".." parent references can't be used as an exploit
  DCHECK(component_path.IsAbsolute());
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIDemoModeAppHost);

  base::flat_set<std::string> webui_resource_paths;
  // Add required resources.
  for (size_t i = 0; i < kAshDemoModeAppResourcesSize; ++i) {
    data_source->AddResourcePath(kAshDemoModeAppResources[i].path,
                                 kAshDemoModeAppResources[i].id);
    webui_resource_paths.insert(kAshDemoModeAppResources[i].path);
  }

  data_source->SetDefaultResource(IDR_ASH_DEMO_MODE_APP_DEMO_MODE_APP_HTML);
  // Add empty string so default resource is still shown for
  // chrome://demo-mode-app
  webui_resource_paths.insert("");

  data_source->SetRequestFilter(
      base::BindRepeating(&ShouldSourceFromComponent, webui_resource_paths),
      base::BindRepeating(&SourceDataFromComponent, component_path));
}

DemoModeAppUI::~DemoModeAppUI() = default;

void DemoModeAppUI::BindInterface(
    mojo::PendingReceiver<mojom::demo_mode::PageHandlerFactory> factory) {
  if (demo_mode_page_factory_.is_bound()) {
    demo_mode_page_factory_.reset();
  }
  demo_mode_page_factory_.Bind(std::move(factory));
}

void DemoModeAppUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::demo_mode::PageHandler> handler) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      web_ui()->GetWebContents()->GetTopLevelNativeWindow());
  demo_mode_page_handler_ =
      std::make_unique<DemoModePageHandler>(std::move(handler), widget);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DemoModeAppUI)

}  // namespace ash
