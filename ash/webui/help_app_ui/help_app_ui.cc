// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/help_app_ui.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_help_app_resources.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/help_app_page_handler.h"
#include "ash/webui/help_app_ui/help_app_untrusted_ui.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/web_applications/webui_test_prod_util.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "chromeos/ash/components/local_search_service/public/mojom/types.mojom.h"
#include "chromeos/constants/url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {
content::WebUIDataSource* CreateAndAddHostDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIHelpAppHost);

  source->SetDefaultResource(IDR_HELP_APP_HOST_INDEX_DARK_LIGHT_HTML);
  source->AddResourcePath("app_icon_192.png", IDR_HELP_APP_ICON_192);
  source->AddResourcePath("app_icon_512.png", IDR_HELP_APP_ICON_512);
  source->AddResourcePath("browser_proxy.js", IDR_HELP_APP_BROWSER_PROXY_JS);

  source->AddLocalizedString("appTitle", IDS_HELP_APP_EXPLORE);
  return source;
}
}  // namespace

HelpAppUI::HelpAppUI(content::WebUI* web_ui,
                     std::unique_ptr<HelpAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source =
      CreateAndAddHostDataSource(browser_context);
  // We need a CSP override to use the chrome-untrusted://, almanac:// and
  // cros-apps:// schemes in the host.
  std::string csp = base::StrCat({"frame-src ", kChromeUIHelpAppUntrustedURL,
                                  " ", chromeos::kAppInstallUriScheme, ": ",
                                  chromeos::kLegacyAppInstallUriScheme, ":;"});
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* permissions_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIHelpAppUntrustedURL));
  permissions_allowlist->RegisterAutoGrantedPermissions(
      untrusted_origin, {
                            ContentSettingsType::COOKIES,
                            ContentSettingsType::IMAGES,
                            ContentSettingsType::JAVASCRIPT,
                            ContentSettingsType::SOUND,
                        });

  if (MaybeConfigureTestableDataSource(host_source)) {
    host_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://resources chrome://webui-test 'self';");
    host_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        std::string("trusted-types test-harness;"));
  }

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* magazine_permissions_allowlist =
      WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin magazine_untrusted_origin =
      url::Origin::Create(GURL(kChromeUIHelpAppKidsMagazineUntrustedURL));
  magazine_permissions_allowlist->RegisterAutoGrantedPermissions(
      magazine_untrusted_origin, {
                                     ContentSettingsType::COOKIES,
                                     ContentSettingsType::IMAGES,
                                     ContentSettingsType::JAVASCRIPT,
                                     ContentSettingsType::SOUND,
                                 });
}

HelpAppUI::~HelpAppUI() = default;

void HelpAppUI::BindInterface(
    mojo::PendingReceiver<help_app::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HelpAppUI::BindInterface(
    mojo::PendingReceiver<local_search_service::mojom::Index> index_receiver) {
  if (base::FeatureList::IsEnabled(features::kEnableLocalSearchService)) {
    auto* const factory = local_search_service::LocalSearchServiceProxyFactory::
        GetForBrowserContext(web_ui()->GetWebContents()->GetBrowserContext());
    factory->SetLocalState(delegate_->GetLocalState());
    factory->GetIndex(local_search_service::IndexId::kHelpApp,
                      local_search_service::Backend::kInvertedIndex,
                      std::move(index_receiver));
  }
}

void HelpAppUI::BindInterface(
    mojo::PendingReceiver<help_app::mojom::SearchHandler> receiver) {
  if (base::FeatureList::IsEnabled(features::kHelpAppLauncherSearch)) {
    help_app::HelpAppManagerFactory::GetForBrowserContext(
        web_ui()->GetWebContents()->GetBrowserContext())
        ->search_handler()
        ->BindInterface(std::move(receiver));
  }
}

void HelpAppUI::CreatePageHandler(
    mojo::PendingReceiver<help_app::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<HelpAppPageHandler>(this, std::move(receiver));
}

bool HelpAppUI::IsJavascriptErrorReportingEnabled() {
  // JavaScript errors are reported via CrashReportPrivate.reportError. Don't
  // send duplicate reports via WebUI.
  return false;
}

WEB_UI_CONTROLLER_TYPE_IMPL(HelpAppUI)

}  // namespace ash
