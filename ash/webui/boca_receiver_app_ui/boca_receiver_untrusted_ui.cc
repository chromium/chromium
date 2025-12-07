// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_ui.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/grit/ash_boca_receiver_app_bundle_resources_map.h"
#include "ash/webui/grit/ash_boca_receiver_untrusted_ui_resources.h"
#include "ash/webui/grit/ash_boca_receiver_untrusted_ui_resources_map.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

BocaReceiverUntrustedUIConfig::BocaReceiverUntrustedUIConfig(
    CreateWebUIControllerFunc create_controller_func)
    : ChromeOSWebUIConfig(content::kChromeUIUntrustedScheme,
                          kBocaReceiverHost,
                          create_controller_func) {}

BocaReceiverUntrustedUIConfig::~BocaReceiverUntrustedUIConfig() = default;

bool BocaReceiverUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsBocaReceiverAppEnabled() ||
         ash::GetChannel() != version_info::Channel::STABLE;
}

BocaReceiverUntrustedUI::BocaReceiverUntrustedUI(
    content::WebUI* web_ui,
    std::unique_ptr<boca_receiver::ReceiverHandlerDelegate> delegate)
    : ui::UntrustedWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUntrustedBocaReceiverURL);
  source->AddResourcePath("", IDR_ASH_BOCA_RECEIVER_UNTRUSTED_UI_INDEX_HTML);
  source->AddResourcePaths(kAshBocaReceiverAppBundleResources);
  source->AddResourcePaths(kAshBocaReceiverUntrustedUiResources);
  source->AddFrameAncestor(GURL(kChromeBocaReceiverURL));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");
  // For testing
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources chrome-untrusted://webui-test "
      "'self';");
}

BocaReceiverUntrustedUI::~BocaReceiverUntrustedUI() = default;

void BocaReceiverUntrustedUI::BindInterface(
    mojo::PendingReceiver<boca_receiver::mojom::UntrustedPageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void BocaReceiverUntrustedUI::CreateUntrustedPageHandler(
    mojo::PendingRemote<boca_receiver::mojom::UntrustedPage> page) {
  page_handler_ =
      std::make_unique<boca_receiver::BocaReceiverUntrustedPageHandler>(
          std::move(page), delegate_.get());
}

WEB_UI_CONTROLLER_TYPE_IMPL(BocaReceiverUntrustedUI)

}  // namespace ash
