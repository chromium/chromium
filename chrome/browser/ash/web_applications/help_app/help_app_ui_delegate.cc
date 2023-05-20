// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_ui_delegate.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace ash {

namespace {
void DeviceInfoCallback(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback,
    apps::DeviceInfo device_info) {
  std::move(callback).Run(help_app::mojom::DeviceInfo::New(
      /*board=*/device_info.board,
      /*model=*/device_info.model,
      /*user_type=*/device_info.user_type));
}
}  // namespace

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui),
      device_info_manager_(std::make_unique<apps::DeviceInfoManager>(
          Profile::FromWebUI(web_ui))) {}

ChromeHelpAppUIDelegate::~ChromeHelpAppUIDelegate() = default;

absl::optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kHelpAppFeedbackCategoryTag[] = "FromHelpApp";
  // We don't change the default description, or add extra diagnostics so those
  // are empty strings.
  chrome::ShowFeedbackPage(GURL(kChromeUIHelpAppURL), profile,
                           chrome::kFeedbackSourceHelpApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kHelpAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
  return absl::nullopt;
}

void ChromeHelpAppUIDelegate::ShowParentalControls() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  // The "People" section of OS Settings contains parental controls.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kPeopleSectionPath);
}

PrefService* ChromeHelpAppUIDelegate::GetLocalState() {
  return g_browser_process->local_state();
}

void ChromeHelpAppUIDelegate::MaybeShowDiscoverNotification() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  UserSessionManager::GetInstance()->MaybeShowHelpAppDiscoverNotification(
      profile);
}

void ChromeHelpAppUIDelegate::MaybeShowReleaseNotesNotification() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  UserSessionManager::GetInstance()->MaybeShowHelpAppReleaseNotesNotification(
      profile);
}

void ChromeHelpAppUIDelegate::GetDeviceInfo(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback) {
  device_info_manager_->GetDeviceInfo(
      base::BindOnce(&DeviceInfoCallback, std::move(callback)));
}

absl::optional<std::string>
ChromeHelpAppUIDelegate::OpenUrlInBrowserAndTriggerInstallDialog(
    const GURL& url) {
  if (!url.is_valid()) {
    return base::StrCat(
        {"ChromeHelpAppUIDelegate::OpenUrlInBrowserAndTriggerInstallDialog "
         "received invalid URL \"",
         url.spec(), "\""});
  } else if (!url.SchemeIs(url::kHttpsScheme)) {
    return base::StrCat(
        {"ChromeHelpAppUIDelegate::OpenUrlInBrowserAndTriggerInstallDialog "
         "received non-HTTPS URL: \"",
         url.spec(), "\""});
  }

  GURL origin_url = GURL(kChromeUIHelpAppUntrustedURL);
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (base::FeatureList::IsEnabled(
          features::kHelpAppAutoTriggerInstallDialog)) {
    // If the feature is enabled, we schedule the following command.
    if (web_app::WebAppProvider::GetForWebApps(profile)) {
      // Web apps are managed in Ash.
      web_app::WebAppProvider* provider =
          web_app::WebAppProvider::GetForWebApps(profile);
      CHECK(provider);
      provider->scheduler().ScheduleNavigateAndTriggerInstallDialog(
          url, origin_url, /*is_renderer_initiated=*/true, base::DoNothing());
    } else {
      // Web apps are managed in Lacros.
      crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
          crosapi::CrosapiManager::Get()
              ->crosapi_ash()
              ->web_app_service_ash()
              ->GetWebAppProviderBridge();
      if (!web_app_provider_bridge) {
        return "ChromeHelpAppUIDelegate::OpenUrlInBrowser "
               "web_app_provider_bridge"
               " not ready";
      }
      web_app_provider_bridge->ScheduleNavigateAndTriggerInstallDialog(
          url, origin_url, /*is_renderer_initiated=*/true);
    }
    return absl::nullopt;
  }

  // We specify a different page transition here because the common
  // `ui::PAGE_TRANSITION_LINK` can be intercepted by URL capturing logic.
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_FROM_API);
  // This method is initiated by the Help app renderer process via Mojo.
  params.is_renderer_initiated = true;
  // The `Navigate` implementation requires renderer-initiated navigations to
  // specify an initiator origin. Set this to chrome-untrusted://help-app.
  params.initiator_origin = url::Origin::Create(origin_url);
  Navigate(&params);

  return absl::nullopt;
}

}  // namespace ash
