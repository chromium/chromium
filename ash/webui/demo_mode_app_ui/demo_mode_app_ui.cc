// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_page_handler.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "ash/webui/grit/ash_demo_mode_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/views/widget/widget.h"

namespace ash {

DemoModeAppUIConfig::DemoModeAppUIConfig()
    : content::WebUIConfig(content::kChromeUIScheme, kChromeUIDemoModeAppHost) {
}

DemoModeAppUIConfig::~DemoModeAppUIConfig() = default;

std::unique_ptr<content::WebUIController>
DemoModeAppUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<DemoModeAppUI>(web_ui);
}

bool DemoModeAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsDemoModeSWAEnabled();
}

DemoModeAppUI::DemoModeAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIDemoModeAppHost);

  // Add required resources.
  for (size_t i = 0; i < kAshDemoModeAppResourcesSize; ++i) {
    html_source->AddResourcePath(kAshDemoModeAppResources[i].path,
                                 kAshDemoModeAppResources[i].id);
  }

  html_source->SetDefaultResource(IDR_ASH_DEMO_MODE_APP_DEMO_MODE_APP_HTML);
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
