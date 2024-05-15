// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

bool BocaUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ash::features::IsBocaEnabled();
}

BocaUI::BocaUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  auto* html_source = content::WebUIDataSource::CreateAndAdd(
      browser_context, ash::kChromeBocaAppHost);

  html_source->AddResourcePath("index.html", IDR_ASH_BOCA_UI_INDEX_HTML);
#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  html_source->SetDefaultResource(IDR_ASH_BOCA_UI_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

BocaUI::~BocaUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BocaUI)
}  // namespace ash
