// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/system_extensions_internals_ui/system_extensions_internals_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_system_extensions_internals_resources.h"
#include "ash/webui/grit/ash_system_extensions_internals_resources_map.h"
#include "ash/webui/system_extensions_internals_ui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

bool SystemExtensionsInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ash::features::kSystemExtensions);
}

SystemExtensionsInternalsUI::SystemExtensionsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUISystemExtensionsInternalsHost);

  data_source->AddResourcePath("",
                               IDR_ASH_SYSTEM_EXTENSIONS_INTERNALS_INDEX_HTML);
  data_source->AddResourcePaths(
      base::make_span(kAshSystemExtensionsInternalsResources,
                      kAshSystemExtensionsInternalsResourcesSize));
}

SystemExtensionsInternalsUI::~SystemExtensionsInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SystemExtensionsInternalsUI)

}  // namespace ash
