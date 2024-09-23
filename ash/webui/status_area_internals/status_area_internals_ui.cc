// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/status_area_internals/status_area_internals_ui.h"

#include <memory>

#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_status_area_internals_resources.h"
#include "ash/webui/grit/ash_status_area_internals_resources_map.h"
#include "ash/webui/status_area_internals/status_area_internals_handler.h"
#include "ash/webui/status_area_internals/url_constants.h"
#include "base/containers/span.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/resource_path.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(source);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);

  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

}  // namespace

StatusAreaInternalsUI::StatusAreaInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://status-area-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIStatusAreaInternalsHost);

  // Add required resources.
  SetupWebUIDataSource(html_source,
                       base::make_span(kAshStatusAreaInternalsResources,
                                       kAshStatusAreaInternalsResourcesSize),
                       IDR_ASH_STATUS_AREA_INTERNALS_MAIN_HTML);
}

StatusAreaInternalsUI::~StatusAreaInternalsUI() = default;

void StatusAreaInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::status_area_internals::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<StatusAreaInternalsHandler>(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(StatusAreaInternalsUI)

StatusAreaInternalsUIConfig::StatusAreaInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         kChromeUIStatusAreaInternalsHost) {}

}  // namespace ash
