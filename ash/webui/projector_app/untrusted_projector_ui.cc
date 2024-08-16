// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/projector_app/untrusted_projector_ui.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/grit/ash_projector_app_untrusted_resources.h"
#include "ash/webui/grit/ash_projector_app_untrusted_resources_map.h"
#include "ash/webui/grit/ash_projector_common_resources.h"
#include "ash/webui/grit/ash_projector_common_resources_map.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"

namespace ash {

namespace {

void CreateAndAddProjectorHTMLSource(content::WebUI* web_ui,
                                     UntrustedProjectorUIDelegate* delegate) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIUntrustedProjectorUrl);

  source->AddResourcePaths(
      base::make_span(kAshProjectorAppUntrustedResources,
                      kAshProjectorAppUntrustedResourcesSize));
  source->AddResourcePaths(base::make_span(kAshProjectorCommonResources,
                                           kAshProjectorCommonResourcesSize));
  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppBundleResources,
                      kChromeosProjectorAppBundleResourcesSize));

  source->AddResourcePath("", IDR_ASH_PROJECTOR_APP_UNTRUSTED_INDEX_HTML);
  source->AddLocalizedString("appTitle", IDS_ASH_PROJECTOR_DISPLAY_SOURCE);

  // Provide a list of specific script resources (javascript files and inlined
  // scripts inside html) or their sha-256 hashes to allow to be executed.
  // "wasm-eval" is added to allow wasm. "chrome-untrusted://resources" is
  // needed to allow the post message api.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources;");
  // Allow fonts.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src https://fonts.gstatic.com;");
  // Allow styles to include inline styling needed for Polymer elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com;");
  std::string mediaCSP =
      std::string("media-src 'self' https://*.drive.google.com ") +
      kChromeUIUntrustedProjectorPwaUrl + " blob:;";
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      // Allows streaming video.
      mediaCSP);
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src blob: data: 'self' https://*.googleusercontent.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' https://www.googleapis.com "
      "https://drive.google.com;");
  // Allow styles to include inline styling needed for Polymer elements and
  // the material 3 dynamic palette.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  delegate->PopulateLoadTimeData(source);
  source->UseStringsJs();

  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIUntrustedProjectorUrl));
  webui_allowlist->RegisterAutoGrantedPermission(untrusted_origin,
                                                 ContentSettingsType::COOKIES);
  webui_allowlist->RegisterAutoGrantedPermission(
      untrusted_origin, ContentSettingsType::JAVASCRIPT);
  webui_allowlist->RegisterAutoGrantedPermission(untrusted_origin,
                                                 ContentSettingsType::IMAGES);
}

}  // namespace

UntrustedProjectorUI::UntrustedProjectorUI(
    content::WebUI* web_ui,
    UntrustedProjectorUIDelegate* delegate,
    PrefService* pref_service)
    : UntrustedWebUIController(web_ui), pref_service_(pref_service) {
  CreateAndAddProjectorHTMLSource(web_ui, delegate);
  ProjectorAppClient::Get()->NotifyAppUIActive(true);
}

UntrustedProjectorUI::~UntrustedProjectorUI() {
  ProjectorAppClient::Get()->NotifyAppUIActive(false);
}

void UntrustedProjectorUI::BindInterface(
    mojo::PendingReceiver<
        projector::mojom::UntrustedProjectorPageHandlerFactory> factory) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(factory));
}

void UntrustedProjectorUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void UntrustedProjectorUI::Create(
    mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
        projector_handler,
    mojo::PendingRemote<projector::mojom::UntrustedProjectorPage> projector) {
  page_handler_ = std::make_unique<UntrustedProjectorPageHandlerImpl>(
      std::move(projector_handler), std::move(projector), pref_service_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(UntrustedProjectorUI)

}  // namespace ash
