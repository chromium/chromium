// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/mall/mall_ui.h"

#include "ash/webui/grit/ash_mall_cros_app_resources.h"
#include "ash/webui/mall/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

bool MallUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ChromeOSWebUIConfig::IsWebUIEnabled(browser_context) &&
         chromeos::features::IsCrosMallSwaEnabled();
}

MallUI::MallUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  auto* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), ash::kChromeUIMallHost);

  source->SetDefaultResource(IDR_ASH_MALL_CROS_APP_INDEX_HTML);
  source->AddResourcePath("mall_icon_192.png",
                          IDR_ASH_MALL_CROS_APP_IMAGES_MALL_ICON_192_PNG);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MallUI)

}  // namespace ash
