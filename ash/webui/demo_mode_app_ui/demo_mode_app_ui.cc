// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_app_ui.h"

#include "ash/webui/demo_mode_app_ui/demo_mode_page_handler.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "ash/webui/grit/ash_demo_mode_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/views/widget/widget.h"

namespace ash {

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
