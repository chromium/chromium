// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/boca_ui/boca_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_app_page_handler.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/boca_ui/webview_auth_delegate.h"
#include "ash/webui/boca_ui/webview_auth_handler.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "ash/webui/grit/ash_boca_ui_resources_map.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_allowlist.h"

namespace ash::boca {

namespace {
content::WebUIDataSource* CreateAndAddHostDataSource(
    content::BrowserContext* browser_context,
    BocaUIDelegate* delegate) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeBocaAppUntrustedURL);

  source->AddResourcePath("", IDR_ASH_BOCA_UI_INDEX_HTML);
  source->AddResourcePaths(kAshBocaUiResources);

  // Resources obtained from CIPD.
  source->AddResourcePaths(kChromeosBocaAppBundleResources);
  return source;
}

}  // namespace

BocaUI::BocaUI(content::WebUI* web_ui,
               std::unique_ptr<BocaUIDelegate> delegate,
               bool is_producer)
    : UntrustedWebUIController(web_ui), is_producer_(is_producer) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source =
      CreateAndAddHostDataSource(browser_context, delegate.get());

  // Allow styles to include inline styling needed for Polymer elements and
  // the material 3 dynamic palette.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");

  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");

  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  // For loading favicon and profile photo.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src 'self' https: data:;");

  // For testing
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources chrome-untrusted://webui-test "
      "'self';");

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
  delegate->PopulateLoadTimeData(host_source);
  host_source->UseStringsJs();

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
  content::BrowserContext* context =
      web_ui()->GetWebContents()->GetBrowserContext();
  CHECK(context);
  const std::string host_name =
      web_ui()->GetWebContents()->GetVisibleURL().host();
  auto auth_handler = std::make_unique<WebviewAuthHandler>(
      std::make_unique<WebviewAuthDelegate>(), context, host_name);
  auto* const profile = Profile::FromWebUI(web_ui());
  auto content_settings_handler =
      std::make_unique<ContentSettingsHandler>(profile);
  auto* const boca_manager =
      ash::BocaManagerFactory::GetInstance()->GetForProfile(profile);
  auto* const on_task_session_manager = boca_manager->GetOnTaskSessionManager();
  auto* const system_web_app_manager =
      on_task_session_manager
          ? on_task_session_manager->GetOnTaskSystemWebAppManager()
          : nullptr;
  BocaAppClient::Get()->GetSessionManager()->OnAppWindowOpened();
  page_handler_impl_ = std::make_unique<BocaAppHandler>(
      std::move(page_handler), std::move(page), web_ui(),
      std::move(auth_handler), std::make_unique<ClassroomPageHandlerImpl>(),
      std::move(content_settings_handler), system_web_app_manager,
      BocaAppClient::Get()->GetSessionManager()->session_client_impl(),
      is_producer_);
  page_handler_impl_->SetSpotlightService(&spotlight_service_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(BocaUI)

}  // namespace ash::boca
