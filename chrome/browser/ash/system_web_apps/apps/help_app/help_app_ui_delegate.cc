// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace ash {

namespace {
void BorealisFeaturesCallback(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback,
    const apps::DeviceInfo& device_info,
    borealis::BorealisFeatures::AllowStatus allow_status) {
  bool is_steam_allowed =
      allow_status == borealis::BorealisFeatures::AllowStatus::kAllowed;
  std::move(callback).Run(help_app::mojom::DeviceInfo::New(
      /*board=*/device_info.board,
      /*model=*/device_info.model,
      /*user_type=*/device_info.user_type,
      /*is_steam_allowed=*/is_steam_allowed));
}

void DeviceInfoCallback(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback,
    base::WeakPtr<Profile> profile,
    apps::DeviceInfo device_info) {
  if (!profile) {
    BorealisFeaturesCallback(
        std::move(callback), device_info,
        borealis::BorealisFeatures::AllowStatus::kFailedToDetermine);
    return;
  }
  auto* borealis_service =
      borealis::BorealisServiceFactory::GetForProfile(profile.get());
  if (!borealis_service) {
    BorealisFeaturesCallback(
        std::move(callback), device_info,
        borealis::BorealisFeatures::AllowStatus::kBlockedOnNonPrimaryProfile);
    return;
  }
  borealis_service->Features().IsAllowed(base::BindOnce(
      &BorealisFeaturesCallback, std::move(callback), device_info));
}
}  // namespace

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeHelpAppUIDelegate::~ChromeHelpAppUIDelegate() = default;

std::optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kHelpAppFeedbackCategoryTag[] = "FromHelpApp";
  // We don't change the default description, or add extra diagnostics so those
  // are empty strings.
  chrome::ShowFeedbackPage(GURL(kChromeUIHelpAppURL), profile,
                           feedback::kFeedbackSourceHelpApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kHelpAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
  return std::nullopt;
}

void ChromeHelpAppUIDelegate::ShowOnDeviceAppControls() {
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      Profile::FromWebUI(web_ui_));
  if (!user) {
    // TODO(crbug.com/447287122): Revisit here to see if we always have the
    // user.
    return;
  }
  // The "Apps" section of OS Settings contains app controls.
  ash::SettingsAppManager::Get()->Open(
      *user, {.sub_page = chromeos::settings::mojom::kAppsSectionPath});
}

void ChromeHelpAppUIDelegate::ShowParentalControls() {
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      Profile::FromWebUI(web_ui_));
  if (!user) {
    // TODO(crbug.com/447287122): Revisit here to see if we always have the
    // user.
    return;
  }
  // The "People" section of OS Settings contains parental controls.
  ash::SettingsAppManager::Get()->Open(
      *user, {.sub_page = chromeos::settings::mojom::kPeopleSectionPath});
}

void ChromeHelpAppUIDelegate::TriggerWelcomeTipCallToAction(
    help_app::mojom::ActionTypeId action_type_id) {
  // TODO(crbug.com/385152937): ScalableIph is under cleanup. This method should
  // be removed too.
  return;
}

PrefService* ChromeHelpAppUIDelegate::GetLocalState() {
  return g_browser_process->local_state();
}

void ChromeHelpAppUIDelegate::LaunchMicrosoft365Setup() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (!chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAllowed(profile)) {
    return;
  }
  ash::cloud_upload::LaunchMicrosoft365Setup(
      profile, web_ui_->GetWebContents()->GetTopLevelNativeWindow());
}

void ChromeHelpAppUIDelegate::MaybeShowReleaseNotesNotification() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  UserSessionManager::GetInstance()->MaybeShowHelpAppReleaseNotesNotification(
      profile);
}

void ChromeHelpAppUIDelegate::GetDeviceInfo(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui_);

  apps::DeviceInfoManager* device_info_manager =
      apps::DeviceInfoManagerFactory::GetForProfile(profile);
  CHECK(device_info_manager);
  device_info_manager->GetDeviceInfo(base::BindOnce(
      &DeviceInfoCallback, std::move(callback), profile->GetWeakPtr()));
}

std::optional<std::string>
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
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile);
    CHECK(provider);
    provider->scheduler().ScheduleNavigateAndTriggerInstallDialog(
        url, origin_url, /*is_renderer_initiated=*/true, base::DoNothing());
    return std::nullopt;
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

  return std::nullopt;
}

void ChromeHelpAppUIDelegate::OpenSettings(
    ash::help_app::mojom::SettingsComponent component) {
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      Profile::FromWebUI(web_ui_));
  if (!user) {
    // TODO(crbug.com/447287122): Revisit here to see if we always have the
    // user.
    return;
  }

  switch (component) {
    case ash::help_app::mojom::SettingsComponent::HOME:
      ash::SettingsAppManager::Get()->Open(*user, {});
      return;
    case ash::help_app::mojom::SettingsComponent::ACCESSIBILITY:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kAccessibilitySectionPath});
      return;
    case ash::help_app::mojom::SettingsComponent::BLUETOOTH:
      ash::SettingsAppManager::Get()->Open(
          *user, {.sub_page =
                      chromeos::settings::mojom::kBluetoothDevicesSubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::DISPLAY:
      ash::SettingsAppManager::Get()->Open(
          *user, {.sub_page = chromeos::settings::mojom::kDisplaySubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::INPUT:
      ash::SettingsAppManager::Get()->Open(
          *user, {.sub_page = chromeos::settings::mojom::kInputSubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::MULTI_DEVICE:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kMultiDeviceSectionPath});
      return;
    case ash::help_app::mojom::SettingsComponent::PEOPLE:
      ash::SettingsAppManager::Get()->Open(
          *user, {.sub_page = chromeos::settings::mojom::kPeopleSectionPath});
      return;
    case ash::help_app::mojom::SettingsComponent::PER_DEVICE_KEYBOARD:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page =
               chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::PER_DEVICE_TOUCHPAD:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page =
               chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::PERSONALIZATION:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kPersonalizationSectionPath});
      return;
    case ash::help_app::mojom::SettingsComponent::PRINTING:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kPrintingDetailsSubpagePath});
      return;
    case ash::help_app::mojom::SettingsComponent::SECURITY_AND_SIGN_IN:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page =
               chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2});
      return;
    case ash::help_app::mojom::SettingsComponent::TOUCHPAD_REVERSE_SCROLLING:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath,
           .setting_id =
               chromeos::settings::mojom::Setting::kTouchpadReverseScrolling});
      return;
    case ash::help_app::mojom::SettingsComponent::TOUCHPAD_SIMULATE_RIGHT_CLICK:
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page = chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath,
           .setting_id = chromeos::settings::mojom::Setting::
               kTouchpadSimulateRightClick});
      return;
  }

  NOTREACHED() << "Invalid settings component value provided";
}

}  // namespace ash
