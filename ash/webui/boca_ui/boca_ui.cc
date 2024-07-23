// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_app_page_handler.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "ash/webui/grit/ash_boca_ui_resources_map.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

content::WebUIDataSource* CreateAndAddHostDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeBocaAppUntrustedURL);

  source->AddResourcePaths(
      base::make_span(kAshBocaUiResources, kAshBocaUiResourcesSize));

  // Resources obtained from CIPD.
  source->AddResourcePaths(base::make_span(
      kChromeosBocaAppBundleResources, kChromeosBocaAppBundleResourcesSize));
  return source;
}

}  // namespace

BocaUI::BocaUI(content::WebUI* web_ui) : UntrustedWebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source =
      CreateAndAddHostDataSource(browser_context);

  // Allow styles to include inline styling needed for Polymer elements and
  // the material 3 dynamic palette.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");

  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* permissions_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeBocaAppUntrustedURL));
  permissions_allowlist->RegisterAutoGrantedPermissions(
      untrusted_origin, {
                            ContentSettingsType::IMAGES,
                            ContentSettingsType::JAVASCRIPT,
                            ContentSettingsType::SOUND,
                        });

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  host_source->SetDefaultResource(IDR_ASH_BOCA_UI_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

BocaUI::~BocaUI() = default;

void BocaUI::BindInterface(
    mojo::PendingReceiver<boca::mojom::BocaPageHandlerFactory> factory) {
  receiver_.reset();
  receiver_.Bind(std::move(factory));
}

void BocaUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void BocaUI::Create(
    mojo::PendingReceiver<boca::mojom::PageHandler> page_handler,
    mojo::PendingRemote<boca::mojom::Page> page) {
  page_handler_impl_ = std::make_unique<BocaAppHandler>(
      this, std::move(page_handler), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(BocaUI)

}  // namespace ash
