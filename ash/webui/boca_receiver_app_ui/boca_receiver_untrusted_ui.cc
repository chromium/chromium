// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "ash/webui/grit/ash_boca_receiver_app_bundle_resources_map.h"
#include "ash/webui/grit/ash_boca_receiver_untrusted_ui_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

BocaReceiverUntrustedUIConfig::BocaReceiverUntrustedUIConfig()
    : content::DefaultWebUIConfig<BocaReceiverUntrustedUI>(
          content::kChromeUIUntrustedScheme,
          kBocaReceiverHost) {}

BocaReceiverUntrustedUIConfig::~BocaReceiverUntrustedUIConfig() = default;

bool BocaReceiverUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  // TODO(crbug.com/435165759): enable based on kiosk policy.
  return features::IsBocaReceiverAppEnabled();
}

BocaReceiverUntrustedUI::BocaReceiverUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUntrustedBocaReceiverURL);
  source->AddResourcePath("", IDR_ASH_BOCA_RECEIVER_UNTRUSTED_UI_INDEX_HTML);
  source->AddResourcePaths(kAshBocaReceiverAppBundleResources);
  source->AddFrameAncestor(GURL(kChromeBocaReceiverURL));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");
}

BocaReceiverUntrustedUI::~BocaReceiverUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BocaReceiverUntrustedUI)

}  // namespace ash
