// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_help_app_ui_delegate.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "url/gurl.h"

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

base::Optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kHelpAppFeedbackCategoryTag[] = "FromHelpApp";
  // We don't change the default description, or add extra diagnostics so those
  // are empty strings.
  chrome::ShowFeedbackPage(GURL(chromeos::kChromeUIHelpAppURL), profile,
                           chrome::kFeedbackSourceHelpApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kHelpAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
  return base::nullopt;
}

void ChromeHelpAppUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  // Enable accessibility mode (slower balloons) if either spoken feedback
  // or switch access is enabled.
  auto* accessibility_manager = ash::AccessibilityManager::Get();
  source->AddBoolean("accessibility",
                     accessibility_manager->IsSpokenFeedbackEnabled() ||
                         accessibility_manager->IsSwitchAccessEnabled());

  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  // Add strings that can be pulled in.
  source->AddString("boardName", base::SysInfo::GetLsbReleaseBoard());
  source->AddString("chromeOSVersion", base::SysInfo::OperatingSystemVersion());
  source->AddString("chromeVersion", chrome::kChromeVersion);
  source->AddInteger("channel", static_cast<int>(chrome::GetChannel()));
  std::string customization_id;
  std::string hwid;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  // MachineStatistics may not exist for browser tests, but it is fine for these
  // to be empty strings.
  provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                &customization_id);
  provider->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid);
  source->AddString("customizationId", customization_id);
  source->AddString("deviceName", ui::GetChromeOSDeviceName());
  source->AddString("hwid", hwid);
  source->AddString("deviceHelpContentId",
                    base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        "device-help-content-id"));

  // Add any features that have been enabled.
  source->AddBoolean("HelpAppReleaseNotes", true);
  source->AddBoolean("HelpAppLauncherSearch",
                     base::FeatureList::IsEnabled(
                         chromeos::features::kHelpAppLauncherSearch) &&
                         base::FeatureList::IsEnabled(
                             chromeos::features::kEnableLocalSearchService));
  source->AddBoolean(
      "HelpAppSearchServiceIntegration",
      base::FeatureList::IsEnabled(
          chromeos::features::kHelpAppSearchServiceIntegration) &&
          base::FeatureList::IsEnabled(
              chromeos::features::kEnableLocalSearchService));

  Profile* profile = Profile::FromWebUI(web_ui_);
  PrefService* pref_service = profile->GetPrefs();

  // Add state from the OOBE flow.
  source->AddBoolean(
      "shouldShowGetStarted",
      pref_service->GetBoolean(prefs::kHelpAppShouldShowGetStarted));
  source->AddBoolean(
      "shouldShowParentalControl",
      pref_service->GetBoolean(prefs::kHelpAppShouldShowParentalControl));
  source->AddBoolean(
      "tabletModeDuringOOBE",
      pref_service->GetBoolean(prefs::kHelpAppTabletModeDuringOobe));
  // Checks if any of the MultiDevice features (e.g. Instant Tethering,
  // Messages, Smart Lock) is allowed on this device.
  source->AddBoolean(
      "multiDeviceFeaturesAllowed",
      chromeos::multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          pref_service));
  source->AddBoolean("tabletMode", ash::TabletMode::Get()->InTabletMode());
  // Checks if there are active touch screens.
  source->AddBoolean(
      "hasTouchScreen",
      !ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty());
  // Checks if the Google Assistant is allowed on this device by going through
  // policies.
  chromeos::assistant::AssistantAllowedState assistant_allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  source->AddBoolean("assistantAllowed",
                     assistant_allowed_state ==
                         chromeos::assistant::AssistantAllowedState::ALLOWED);
  source->AddBoolean(
      "assistantEnabled",
      ash::AssistantState::Get()->settings_enabled().value_or(false));
  source->AddBoolean("playStoreEnabled",
                     arc::IsArcPlayStoreEnabledForProfile(profile));
  source->AddBoolean("pinEnabled",
                     chromeos::quick_unlock::IsPinEnabled(pref_service));

  // Data about what type of account/login this is.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  source->AddBoolean("isManagedDevice",
                     profile->GetProfilePolicyConnector()->IsManaged());
  source->AddInteger("userType", user_manager->GetActiveUser()->GetType());
  source->AddBoolean("isEphemeralUser",
                     user_manager->IsCurrentUserNonCryptohomeDataEphemeral());
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
