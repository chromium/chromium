// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_ui.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_net_log.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/shared/webui_shared.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_fre_resources.h"
#include "chrome/grit/glic_fre_resources_map.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_allowlist.h"
#include "ui/webui/webui_util.h"

namespace glic {

// static
bool GlicUI::simulate_no_connection_ = false;

GlicUIConfig::GlicUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIGlicHost) {}

bool GlicUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
      Profile::FromBrowserContext(browser_context));
}

GlicUI::GlicUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"closeButtonLabel", IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL},
      {"errorNotice", IDS_GLIC_ERROR_NOTICE},
      {"errorNoticeActionButton", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"ineligibleProfileNotice", IDS_GLIC_INELIGIBLE_PROFILE_NOTICE},
      {"ineligibleProfileNoticeActionButton",
       IDS_GLIC_INELIGIBLE_PROFILE_NOTICE_ACTION_BUTTON},
      {"ineligibleProfileNoticeHeader",
       IDS_GLIC_INELIGIBLE_PROFILE_NOTICE_HEADER},
      {"disabledByAdminNotice", IDS_GLIC_DISABLED_BY_ADMIN_NOTICE},
      {"disabledByAdminNoticeCloseButton",
       IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_CLOSE_BUTTON},
      {"disabledByAdminNoticeHeader", IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_HEADER},
      {"offlineNoticeAction", IDS_GLIC_OFFLINE_NOTICE_ACTION},
      {"offlineNoticeActionButton", IDS_GLIC_OFFLINE_NOTICE_ACTION_BUTTON},
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
      {"signInNotice", IDS_GLIC_SIGN_IN_NOTICE},
      {"signInNoticeActionButton", IDS_GLIC_SIGN_IN_NOTICE_ACTION_BUTTON},
      {"signInNoticeHeader", IDS_GLIC_SIGN_IN_NOTICE_HEADER},
      {"unresponsiveMessage", IDS_GLIC_UNRESPONSIVE_MESSAGE},
  };

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Set up the chrome://glic source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIGlicHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicResources, IDR_GLIC_GLIC_HTML);
  ConfigureSharedWebUISource(*source);

  for (const auto& resource : kGlicFreResources) {
    source->AddResourcePath(base::StrCat({"fre/", resource.path}), resource.id);
  }

  // Add localized strings.
  source->AddLocalizedStrings(kStrings);

  // Add parameterized admin notice string.
  source->AddString("disabledByAdminNoticeWithLink",
                    l10n_util::GetStringFUTF16(
                        IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_WITH_LINK,
                        base::UTF8ToUTF16(features::kGlicCaaLinkUrl.Get()),
                        base::UTF8ToUTF16(features::kGlicCaaLinkText.Get())));

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  allowlist->RegisterAutoGrantedPermission(source->GetOrigin(),
                                           ContentSettingsType::GEOLOCATION);

  auto* command_line = base::CommandLine::ForCurrentProcess();

  source->AddBoolean("loggingEnabled",
                     command_line->HasSwitch(::switches::kGlicHostLogging));

  // Set up guest URL via cli flag or default to finch param value.
  const GURL guest_url = GetGuestURL();
  source->AddString("glicGuestURL", guest_url.spec());
  net_log::LogDummyNetworkRequestForTrafficAnnotation(guest_url,
                                                      net_log::GlicPage::kGlic);
  source->AddBoolean("simulateNoConnection", simulate_no_connection_);

  source->AddResourcePath("glic_logo.svg", GetResourceID(IDR_GLIC_LOGO));

  // Set up guest api source.
  // This comes from 'glic_api_injection' in
  // chrome/browser/resources/glic/BUILD.gn.
  source->AddString(
      "glicGuestAPISource",
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_GLIC_API_IMPL_GLIC_API_INJECTED_CLIENT_ROLLUP_JS));

  std::string allowed_origins =
      command_line->GetSwitchValueASCII(::switches::kGlicAllowedOrigins);
  if (allowed_origins.empty()) {
    allowed_origins = features::kGlicAllowedOriginsOverride.Get();
  }

  // Allow corp origins for @google accounts.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (identity_manager && IsPrimaryAccountGoogleInternal(*identity_manager)) {
    allowed_origins += " https://*.corp.google.com";
  }

  source->AddString("glicAllowedOrigins", allowed_origins);

  bool reload_after_navigation =
      !command_line->HasSwitch(::switches::kGlicSkipReloadAfterNavigation);
  source->AddBoolean("reloadAfterNavigation", reload_after_navigation);

  source->AddBoolean(
      "ignoreOfflineState",
      base::FeatureList::IsEnabled(features::kGlicIgnoreOfflineState));

  source->AddBoolean("enableDebug",
                     base::FeatureList::IsEnabled(features::kGlicDebugWebview));

  // Set up for periodic web client responsiveness check and its interval,
  // timeout, and max unresponsive ui time.
  source->AddBoolean(
      "isClientResponsivenessCheckEnabled",
      base::FeatureList::IsEnabled(features::kGlicClientResponsivenessCheck));
  source->AddInteger("clientResponsivenessCheckIntervalMs",
                     features::kGlicClientResponsivenessCheckIntervalMs.Get());
  source->AddInteger("clientResponsivenessCheckTimeoutMs",
                     features::kGlicClientResponsivenessCheckTimeoutMs.Get());
  source->AddInteger("clientUnresponsiveUiMaxTimeMs",
                     features::kGlicClientUnresponsiveUiMaxTimeMs.Get());
  source->AddBoolean(
      "clientResponsivenessCheckIgnoreWhenDebuggerAttached",
      features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached.Get());
  source->AddBoolean("enableWebClientUnresponsiveMetrics",
                     base::FeatureList::IsEnabled(
                         features::kGlicWebClientUnresponsiveMetrics));
  std::string admin_blocked_redirect_patterns;
  if (base::FeatureList::IsEnabled(features::kGlicCaaGuestError)) {
    admin_blocked_redirect_patterns = command_line->GetSwitchValueASCII(
        ::switches::kGlicAdminRedirectPatterns);
    if (admin_blocked_redirect_patterns.empty()) {
      admin_blocked_redirect_patterns =
          features::kGlicCaaGuestRedirectPatterns.Get();
    }
  }
  source->AddString("adminBlockedRedirectPatterns",
                    admin_blocked_redirect_patterns);

  source->AddString(
      "glicFreURL",
      GetFreURL(Profile::FromBrowserContext(browser_context)).spec());
  source->AddBoolean("isUnifiedFre",
                     GlicEnabling::IsUnifiedFreEnabled(
                         Profile::FromBrowserContext(browser_context)));
  source->AddBoolean(
      "shouldShowFre",
      !base::FeatureList::IsEnabled(features::kGlicTrustFirstOnboarding) &&
          !GlicEnabling::HasConsentedForProfile(
              Profile::FromBrowserContext(browser_context)));
  source->AddInteger("reloadMaxLoadingTimeMs",
                     features::kGlicReloadMaxLoadingTimeMs.Get());
  source->AddBoolean("caaGuestError", base::FeatureList::IsEnabled(
                                          features::kGlicCaaGuestError));
  source->AddBoolean(
      "glicPopupWindowsEnabled",
      base::FeatureList::IsEnabled(features::kGlicPopupWindowsEnabled));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicUI)

GlicUI::~GlicUI() = default;

void GlicUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::FrePageHandlerFactory> receiver) {
  fre_page_factory_receiver_.reset();
  fre_page_factory_receiver_.Bind(std::move(receiver));
}

void GlicUI::CreatePageHandler(
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<glic::mojom::Page> page) {
  page_handler_ = std::make_unique<GlicPageHandler>(
      web_ui()->GetWebContents(), std::move(receiver), std::move(page));
}

void GlicUI::CreatePageHandler(
    mojo::PendingReceiver<glic::mojom::FrePageHandler> fre_receiver) {
  fre_page_handler_ = std::make_unique<GlicFrePageHandler>(
      /*is_unified_fre=*/true, web_ui()->GetWebContents(),
      std::move(fre_receiver));
}

}  // namespace glic
