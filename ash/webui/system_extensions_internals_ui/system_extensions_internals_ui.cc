// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/system_extensions_internals_ui/system_extensions_internals_ui.h"

#include "ash/grit/ash_system_extensions_internals_resources.h"
#include "ash/grit/ash_system_extensions_internals_resources_map.h"
#include "ash/webui/system_extensions_internals_ui/url_constants.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

SystemExtensionsInternalsUI::SystemExtensionsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto data_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUISystemExtensionsInternalsHost));

  data_source->AddResourcePath("",
                               IDR_ASH_SYSTEM_EXTENSIONS_INTERNALS_INDEX_HTML);
  data_source->AddResourcePaths(
      base::make_span(kAshSystemExtensionsInternalsResources,
                      kAshSystemExtensionsInternalsResourcesSize));

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, data_source.release());
}

SystemExtensionsInternalsUI::~SystemExtensionsInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SystemExtensionsInternalsUI)

}  // namespace ash
