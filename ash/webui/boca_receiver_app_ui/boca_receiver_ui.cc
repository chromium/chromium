// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "ash/webui/grit/ash_boca_receiver_ui_resources.h"
#include "ash/webui/grit/ash_boca_receiver_ui_resources_map.h"
#include "base/strings/stringprintf.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash {

BocaReceiverUIConfig::BocaReceiverUIConfig()
    : content::DefaultWebUIConfig<BocaReceiverUI>(content::kChromeUIScheme,
                                                  kBocaReceiverHost) {}

BocaReceiverUIConfig::~BocaReceiverUIConfig() = default;

bool BocaReceiverUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsBocaReceiverAppEnabled() ||
         ash::GetChannel() != version_info::Channel::STABLE;
}

BocaReceiverUI::BocaReceiverUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kBocaReceiverHost);
  source->AddResourcePath("", IDR_ASH_BOCA_RECEIVER_UI_INDEX_HTML);
  source->AddResourcePaths(kAshBocaReceiverUiResources);
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StringPrintf("frame-src %s;", kChromeUntrustedBocaReceiverURL));

  static constexpr webui::LocalizedString kStrings[] = {
      {"appTitle", IDS_BOCA_RECEIVER_TITLE},
  };
  source->AddLocalizedStrings(kStrings);
}

BocaReceiverUI::~BocaReceiverUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BocaReceiverUI)

}  // namespace ash
