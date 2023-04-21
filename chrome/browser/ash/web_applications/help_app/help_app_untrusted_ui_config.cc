// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_untrusted_ui_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/webui/help_app_ui/help_app_untrusted_ui.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

void PopulateLoadTimeData(content::WebUI* web_ui,
                          content::WebUIDataSource* source) {
  // Enable accessibility mode (slower balloons) if either spoken feedback
  // or switch access is enabled.
  auto* accessibility_manager = AccessibilityManager::Get();
  source->AddBoolean("accessibility",
                     accessibility_manager->IsSpokenFeedbackEnabled() ||
                         accessibility_manager->IsSwitchAccessEnabled());

  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  source->AddBoolean("isLowEndDevice", base::SysInfo::IsLowEndDevice());
  // Add strings that can be pulled in.
  source->AddString("boardName", base::SysInfo::GetLsbReleaseBoard());
  source->AddString("chromeOSVersion", base::SysInfo::OperatingSystemVersion());
  source->AddString("chromeVersion", chrome::kChromeVersion);
  source->AddInteger("channel", static_cast<int>(chrome::GetChannel()));
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  // MachineStatistics may not exist for browser tests, but it is fine for these
  // to be empty strings.
  const absl::optional<base::StringPiece> customization_id =
      provider->GetMachineStatistic(system::kCustomizationIdKey);
  const absl::optional<base::StringPiece> hwid =
      provider->GetMachineStatistic(system::kHardwareClassKey);
  source->AddString("customizationId",
                    std::string(customization_id.value_or("")));
  source->AddString("deviceName", ui::GetChromeOSDeviceName());
  source->AddString("hwid", std::string(hwid.value_or("")));
  source->AddString("deviceHelpContentId",
                    base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        "device-help-content-id"));

  // Add any features that have been enabled.
  source->AddBoolean("colorThemes", true);
  source->AddBoolean("HelpAppReleaseNotes", true);
  source->AddBoolean(
      "HelpAppLauncherSearch",
      base::FeatureList::IsEnabled(features::kHelpAppLauncherSearch) &&
          base::FeatureList::IsEnabled(features::kEnableLocalSearchService));
  source->AddBoolean(
      "HelpAppAppsDiscovery",
      base::FeatureList::IsEnabled(features::kHelpAppAppsDiscovery));
  source->AddBoolean(
      "HelpAppSearchServiceIntegration",
      base::FeatureList::IsEnabled(features::kEnableLocalSearchService));
  source->AddBoolean("isCloudGamingDevice",
                     chromeos::features::IsCloudGamingDeviceEnabled());

  Profile* profile = Profile::FromWebUI(web_ui);
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
      multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(pref_service));
  source->AddBoolean("tabletMode", TabletMode::Get()->InTabletMode());
  // Whether or not RGB Keyboard is supported and configurable from the
  // Personalization Hub.
  RgbKeyboardManager* rgb_keyboard_manager =
      Shell::Get()->rgb_keyboard_manager();
  source->AddBoolean(
      "rgbKeyboard",
      rgb_keyboard_manager && rgb_keyboard_manager->IsRgbKeyboardSupported());

  // Checks if there are active touch screens.
  source->AddBoolean(
      "hasTouchScreen",
      !ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty());
  // Checks if the Google Assistant is allowed on this device by going through
  // policies.
  assistant::AssistantAllowedState assistant_allowed_state =
      ::assistant::IsAssistantAllowedForProfile(profile);
  source->AddBoolean(
      "assistantAllowed",
      assistant_allowed_state == assistant::AssistantAllowedState::ALLOWED);
  source->AddBoolean("assistantEnabled",
                     AssistantState::Get()->settings_enabled().value_or(false));
  source->AddBoolean("playStoreEnabled",
                     arc::IsArcPlayStoreEnabledForProfile(profile));
  source->AddBoolean("pinEnabled", quick_unlock::IsPinEnabled());

  // Data about what type of account/login this is.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  source->AddBoolean("isManagedDevice",
                     profile->GetProfilePolicyConnector()->IsManaged());
  source->AddInteger("userType", user_manager->GetActiveUser()->GetType());
  source->AddBoolean("isEphemeralUser",
                     user_manager->IsCurrentUserNonCryptohomeDataEphemeral());
}

}  // namespace

HelpAppUntrustedUIConfig::HelpAppUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, kChromeUIHelpAppHost) {}

HelpAppUntrustedUIConfig::~HelpAppUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
HelpAppUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                const GURL& url) {
  base::RepeatingCallback<void(content::WebUIDataSource*)> callback =
      base::BindRepeating(&PopulateLoadTimeData, web_ui);

  return std::make_unique<HelpAppUntrustedUI>(web_ui, callback);
}

}  // namespace ash
