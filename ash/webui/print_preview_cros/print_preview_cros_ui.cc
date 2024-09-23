// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/print_preview_cros/print_preview_cros_ui.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_print_preview_cros_app_resources.h"
#include "ash/webui/grit/ash_print_preview_cros_app_resources_map.h"
#include "ash/webui/print_preview_cros/backend/destination_provider.h"
#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "base/containers/span.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::printing::print_preview {

namespace {

void ConfigurePolicies(content::WebUIDataSource* source) {
  // Enable common and test resources.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  // Configure ash specific trusted type polices.
  ash::EnableTrustedTypesCSP(source);
}

// Setup app resources and ensure default resource is the app index page.
void ConfigureResources(content::WebUIDataSource* source,
                        int default_resource) {
  const auto resources = base::make_span(kAshPrintPreviewCrosAppResources,
                                         kAshPrintPreviewCrosAppResourcesSize);
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("", default_resource);
}

// Setup common test resources used in browser tests.
void ConfigureTestResources(content::WebUIDataSource* source) {
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

}  // namespace

bool PrintPreviewCrosUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ChromeOSWebUIConfig::IsWebUIEnabled(browser_context) &&
         ash::features::IsPrinterPreviewCrosAppEnabled();
}

PrintPreviewCrosUI::PrintPreviewCrosUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  // Configure resources.
  auto* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      ash::kChromeUIPrintPreviewCrosHost);
  ConfigurePolicies(source);
  ConfigureResources(source, IDR_ASH_PRINT_PREVIEW_CROS_APP_INDEX_HTML);
  ConfigureTestResources(source);
}

PrintPreviewCrosUI::~PrintPreviewCrosUI() = default;

void PrintPreviewCrosUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void PrintPreviewCrosUI::BindInterface(
    mojo::PendingReceiver<mojom::DestinationProvider> receiver) {
  destination_provider_ = std::make_unique<DestinationProvider>();
  destination_provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrintPreviewCrosUI)

}  // namespace ash::printing::print_preview
