// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/mall/mall_ui.h"

#include <memory>

#include "ash/webui/grit/ash_mall_cros_app_resources.h"
#include "ash/webui/grit/ash_mall_cros_app_resources_map.h"
#include "ash/webui/mall/mall_page_handler.h"
#include "ash/webui/mall/mall_ui_delegate.h"
#include "ash/webui/mall/url_constants.h"
#include "base/strings/strcat.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

bool MallUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ChromeOSWebUIConfig::IsWebUIEnabled(browser_context) &&
         chromeos::features::IsCrosMallSwaEnabled();
}

MallUI::MallUI(content::WebUI* web_ui, std::unique_ptr<MallUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  auto* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), ash::kChromeUIMallHost);
  source->SetDefaultResource(IDR_ASH_MALL_CROS_APP_INDEX_HTML);
  source->AddLocalizedString("message", IDS_ERRORPAGES_HEADING_YOU_ARE_OFFLINE);
  source->AddResourcePaths(
      base::make_span(kAshMallCrosAppResources, kAshMallCrosAppResourcesSize));

  // We need a CSP override to be able to embed the Mall website, and to handle
  // cros-apps:// links to install apps.
  std::string csp = base::StrCat({"frame-src ", GetMallBaseUrl().spec(), " ",
                                  chromeos::kAppInstallUriScheme, ": ",
                                  chromeos::kLegacyAppInstallUriScheme, ":;"});
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
}

MallUI::~MallUI() = default;

void MallUI::BindInterface(
    mojo::PendingReceiver<mall::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<MallPageHandler>(std::move(receiver), *delegate_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MallUI)

}  // namespace ash
