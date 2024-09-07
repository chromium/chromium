// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"

#include <string_view>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/devicetype.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/screen.h"

using ash::NetworkHandler;

namespace crosapi {

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the customization ID setting.
const char kPropertyCustomizationID[] = "customizationId";

// Key which corresponds to the oem_device_requisition setting.
const char kPropertyDeviceRequisition[] = "deviceRequisition";

// Key which corresponds to the isMeetDevice property in JS.
const char kPropertyMeetDevice[] = "isMeetDevice";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

// Key which corresponds to the board property in JS.
const char kPropertyBoard[] = "board";

// Key which corresponds to the isOwner property in JS.
const char kPropertyOwner[] = "isOwner";

// Key which corresponds to the clientId property in JS.
const char kPropertyClientId[] = "clientId";

// Key which corresponds to the timezone property in JS.
const char kPropertySupportedTimezones[] = "supportedTimezones";

// Key which corresponds to the large cursor A11Y property in JS.
const char kPropertyLargeCursorEnabled[] = "a11yLargeCursorEnabled";

// Key which corresponds to the sticky keys A11Y property in JS.
const char kPropertyStickyKeysEnabled[] = "a11yStickyKeysEnabled";

// Key which corresponds to the spoken feedback A11Y property in JS.
const char kPropertySpokenFeedbackEnabled[] = "a11ySpokenFeedbackEnabled";

// Key which corresponds to the high contrast mode A11Y property in JS.
const char kPropertyHighContrastEnabled[] = "a11yHighContrastEnabled";

// Key which corresponds to the screen magnifier A11Y property in JS.
const char kPropertyScreenMagnifierEnabled[] = "a11yScreenMagnifierEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyAutoclickEnabled[] = "a11yAutoClickEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyVirtualKeyboardEnabled[] = "a11yVirtualKeyboardEnabled";

// Key which corresponds to the caret highlight A11Y property in JS.
const char kPropertyCaretHighlightEnabled[] = "a11yCaretHighlightEnabled";

// Key which corresponds to the cursor highlight A11Y property in JS.
const char kPropertyCursorHighlightEnabled[] = "a11yCursorHighlightEnabled";

// Key which corresponds to the focus highlight A11Y property in JS.
const char kPropertyFocusHighlightEnabled[] = "a11yFocusHighlightEnabled";

// Key which corresponds to the select-to-speak A11Y property in JS.
const char kPropertySelectToSpeakEnabled[] = "a11ySelectToSpeakEnabled";

// Key which corresponds to the Switch Access A11Y property in JS.
const char kPropertySwitchAccessEnabled[] = "a11ySwitchAccessEnabled";

// Key which corresponds to the cursor color A11Y property in JS.
const char kPropertyCursorColorEnabled[] = "a11yCursorColorEnabled";

// Key which corresponds to the docked magnifier property in JS.
const char kPropertyDockedMagnifierEnabled[] = "a11yDockedMagnifierEnabled";

// Key which corresponds to the send-function-keys property in JS.
const char kPropertySendFunctionsKeys[] = "sendFunctionKeys";

// Key which corresponds to the sessionType property in JS.
const char kPropertySessionType[] = "sessionType";

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Key which corresponds to the "kiosk" value of the SessionType enum in JS.
const char kSessionTypeKiosk[] = "kiosk";

// Key which corresponds to the "public session" value of the SessionType enum
// in JS.
const char kSessionTypePublicSession[] = "public session";

// Key which corresponds to the "normal" value of the SessionType enum in JS.
const char kSessionTypeNormal[] = "normal";

// Key which corresponds to the playStoreStatus property in JS.
const char kPropertyPlayStoreStatus[] = "playStoreStatus";

// Key which corresponds to the "not available" value of the PlayStoreStatus
// enum in JS.
const char kPlayStoreStatusNotAvailable[] = "not available";

// Key which corresponds to the "available" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusAvailable[] = "available";

// Key which corresponds to the "enabled" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusEnabled[] = "enabled";

// Key which corresponds to the managedDeviceStatus property in JS.
const char kPropertyManagedDeviceStatus[] = "managedDeviceStatus";

// Value to which managedDeviceStatus property is set for unmanaged devices.
const char kManagedDeviceStatusNotManaged[] = "not managed";

// Value to which managedDeviceStatus property is set for managed devices.
const char kManagedDeviceStatusManaged[] = "managed";

// Key which corresponds to the deviceType property in JS.
const char kPropertyDeviceType[] = "deviceType";

// Value to which deviceType property is set for Chromebase.
const char kDeviceTypeChromebase[] = "chromebase";

// Value to which deviceType property is set for Chromebit.
const char kDeviceTypeChromebit[] = "chromebit";

// Value to which deviceType property is set for Chromebook.
const char kDeviceTypeChromebook[] = "chromebook";

// Value to which deviceType property is set for Chromebox.
const char kDeviceTypeChromebox[] = "chromebox";

// Value to which deviceType property is set when the specific type is unknown.
const char kDeviceTypeChromedevice[] = "chromedevice";

// Key which corresponds to the stylusStatus property in JS.
const char kPropertyStylusStatus[] = "stylusStatus";

// Value to which stylusStatus property is set when the device does not support
// stylus input.
const char kStylusStatusUnsupported[] = "unsupported";

// Value to which stylusStatus property is set when the device supports stylus
// input, but no stylus has been seen before.
const char kStylusStatusSupported[] = "supported";

// Value to which stylusStatus property is set when the device has a built-in
// stylus or a stylus has been seen before.
const char kStylusStatusSeen[] = "seen";

// Key which corresponds to the assistantStatus property in JS.
const char kPropertyAssistantStatus[] = "assistantStatus";

// Value to which assistantStatus property is set when the device supports
// Assistant.
const char kAssistantStatusSupported[] = "supported";

const struct {
  const char* api_name;
  const char* preference_name;
} kPreferencesMap[] = {
    {kPropertyLargeCursorEnabled, ash::prefs::kAccessibilityLargeCursorEnabled},
    {kPropertyStickyKeysEnabled, ash::prefs::kAccessibilityStickyKeysEnabled},
    {kPropertySpokenFeedbackEnabled,
     ash::prefs::kAccessibilitySpokenFeedbackEnabled},
    {kPropertyHighContrastEnabled,
     ash::prefs::kAccessibilityHighContrastEnabled},
    {kPropertyScreenMagnifierEnabled,
     ash::prefs::kAccessibilityScreenMagnifierEnabled},
    {kPropertyAutoclickEnabled, ash::prefs::kAccessibilityAutoclickEnabled},
    {kPropertyVirtualKeyboardEnabled,
     ash::prefs::kAccessibilityVirtualKeyboardEnabled},
    {kPropertyCaretHighlightEnabled,
     ash::prefs::kAccessibilityCaretHighlightEnabled},
    {kPropertyCursorHighlightEnabled,
     ash::prefs::kAccessibilityCursorHighlightEnabled},
    {kPropertyFocusHighlightEnabled,
     ash::prefs::kAccessibilityFocusHighlightEnabled},
    {kPropertySelectToSpeakEnabled,
     ash::prefs::kAccessibilitySelectToSpeakEnabled},
    {kPropertySwitchAccessEnabled,
     ash::prefs::kAccessibilitySwitchAccessEnabled},
    {kPropertyCursorColorEnabled, ash::prefs::kAccessibilityCursorColorEnabled},
    {kPropertyDockedMagnifierEnabled, ash::prefs::kDockedMagnifierEnabled},
    {kPropertySendFunctionsKeys, ash::prefs::kSendFunctionKeys}};

bool IsEnterpriseKiosk() {
  if (!IsRunningInForcedAppMode()) {
    return false;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
}

std::string GetClientId() {
  return IsEnterpriseKiosk()
             ? g_browser_process->metrics_service()->GetClientId()
             : std::string();
}

const char* GetBoolPrefNameForApiProperty(const char* api_name) {
  for (const auto& item : kPreferencesMap) {
    if (strcmp(item.api_name, api_name) == 0) {
      return item.preference_name;
    }
  }

  return nullptr;
}

std::unique_ptr<base::Value> GetValue(const std::string& property_name) {
  if (property_name == kPropertyHWID) {
    ash::system::StatisticsProvider* provider =
        ash::system::StatisticsProvider::GetInstance();
    const std::optional<std::string_view> hwid =
        provider->GetMachineStatistic(ash::system::kHardwareClassKey);
    return std::make_unique<base::Value>(hwid.value_or(""));
  }

  if (property_name == kPropertyCustomizationID) {
    ash::system::StatisticsProvider* provider =
        ash::system::StatisticsProvider::GetInstance();
    const std::optional<std::string_view> customization_id =
        provider->GetMachineStatistic(ash::system::kCustomizationIdKey);
    return std::make_unique<base::Value>(customization_id.value_or(""));
  }

  if (property_name == kPropertyDeviceRequisition) {
    ash::system::StatisticsProvider* provider =
        ash::system::StatisticsProvider::GetInstance();
    const std::optional<std::string_view> device_requisition =
        provider->GetMachineStatistic(ash::system::kOemDeviceRequisitionKey);
    return std::make_unique<base::Value>(device_requisition.value_or(""));
  }

  if (property_name == kPropertyMeetDevice) {
#if BUILDFLAG(PLATFORM_CFM)
    return std::make_unique<base::Value>(true);
#else
    return std::make_unique<base::Value>(false);
#endif
  }

  if (property_name == kPropertyHomeProvider) {
    const ash::DeviceState* cellular_device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            ash::NetworkTypePattern::Cellular());
    std::string home_provider_id;
    if (cellular_device) {
      if (!cellular_device->country_code().empty()) {
        home_provider_id = base::StringPrintf(
            "%s (%s)", cellular_device->operator_name().c_str(),
            cellular_device->country_code().c_str());
      } else {
        home_provider_id = cellular_device->operator_name();
      }
    }
    return std::make_unique<base::Value>(home_provider_id);
  }

  if (property_name == kPropertyInitialLocale) {
    return std::make_unique<base::Value>(ash::StartupUtils::GetInitialLocale());
  }

  if (property_name == kPropertyBoard) {
    return std::make_unique<base::Value>(base::SysInfo::GetLsbReleaseBoard());
  }

  if (property_name == kPropertyOwner) {
    return std::make_unique<base::Value>(
        user_manager::UserManager::Get()->IsCurrentUserOwner());
  }

  if (property_name == kPropertySessionType) {
    if (extensions::ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
      return std::make_unique<base::Value>(kSessionTypeKiosk);
    if (extensions::ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount())
      return std::make_unique<base::Value>(kSessionTypePublicSession);
    return std::make_unique<base::Value>(kSessionTypeNormal);
  }

  if (property_name == kPropertyPlayStoreStatus) {
    if (arc::IsArcAllowedForProfile(ProfileManager::GetPrimaryUserProfile()))
      return std::make_unique<base::Value>(kPlayStoreStatusEnabled);
    if (arc::IsArcAvailable())
      return std::make_unique<base::Value>(kPlayStoreStatusAvailable);
    return std::make_unique<base::Value>(kPlayStoreStatusNotAvailable);
  }

  if (property_name == kPropertyManagedDeviceStatus) {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    if (connector->IsDeviceEnterpriseManaged()) {
      return std::make_unique<base::Value>(kManagedDeviceStatusManaged);
    }
    return std::make_unique<base::Value>(kManagedDeviceStatusNotManaged);
  }

  if (property_name == kPropertyDeviceType) {
    switch (chromeos::GetDeviceType()) {
      case chromeos::DeviceType::kChromebox:
        return std::make_unique<base::Value>(kDeviceTypeChromebox);
      case chromeos::DeviceType::kChromebase:
        return std::make_unique<base::Value>(kDeviceTypeChromebase);
      case chromeos::DeviceType::kChromebit:
        return std::make_unique<base::Value>(kDeviceTypeChromebit);
      case chromeos::DeviceType::kChromebook:
        return std::make_unique<base::Value>(kDeviceTypeChromebook);
      default:
        return std::make_unique<base::Value>(kDeviceTypeChromedevice);
    }
  }

  if (property_name == kPropertyStylusStatus) {
    if (!ash::stylus_utils::HasStylusInput()) {
      return std::make_unique<base::Value>(kStylusStatusUnsupported);
    }

    bool seen = g_browser_process->local_state()->HasPrefPath(
        ash::prefs::kHasSeenStylus);
    return std::make_unique<base::Value>(seen ? kStylusStatusSeen
                                              : kStylusStatusSupported);
  }

  if (property_name == kPropertyAssistantStatus) {
    return std::make_unique<base::Value>(kAssistantStatusSupported);
  }

  if (property_name == kPropertyClientId) {
    return std::make_unique<base::Value>(GetClientId());
  }

  if (property_name == kPropertyTimezone) {
    if (ash::system::PerUserTimezoneEnabled()) {
      const PrefService::Preference* timezone =
          ProfileManager::GetPrimaryUserProfile()->GetPrefs()->FindPreference(
              prefs::kUserTimezone);
      return std::make_unique<base::Value>(timezone->GetValue()->Clone());
    }
    // TODO(crbug.com/40508978): Convert CrosSettings::Get to take a unique_ptr.
    return base::Value::ToUniquePtrValue(
        ash::CrosSettings::Get()->GetPref(ash::kSystemTimezone)->Clone());
  }

  if (property_name == kPropertySupportedTimezones) {
    return base::Value::ToUniquePtrValue(
        base::Value(ash::system::GetTimezoneList()));
  }

  const char* pref_name = GetBoolPrefNameForApiProperty(property_name.c_str());
  if (pref_name) {
    return std::make_unique<base::Value>(
        ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
            pref_name));
  }

  DLOG(ERROR) << "Unknown property request: " << property_name;
  return nullptr;
}

}  // namespace

ExtensionInfoPrivateAsh::ExtensionInfoPrivateAsh() = default;
ExtensionInfoPrivateAsh::~ExtensionInfoPrivateAsh() = default;

void ExtensionInfoPrivateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ExtensionInfoPrivate> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ExtensionInfoPrivateAsh::GetSystemProperties(
    const std::vector<std::string>& property_names,
    GetSystemPropertiesCallback callback) {
  base::Value::Dict result;
  for (const std::string& property_name : property_names) {
    std::unique_ptr<base::Value> value = GetValue(property_name);
    if (value) {
      result.Set(property_name,
                 base::Value::FromUniquePtrValue(std::move(value)));
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Value(std::move(result))));
}

void ExtensionInfoPrivateAsh::SetTimezone(const std::string& value) {
  if (ash::system::PerUserTimezoneEnabled()) {
    ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetString(
        prefs::kUserTimezone, value);
  } else {
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(
            ProfileManager::GetPrimaryUserProfile());
    if (user)
      ash::system::SetSystemTimezone(user, value);
  }
}

void ExtensionInfoPrivateAsh::SetBool(const std::string& property_name,
                                      bool value,
                                      SetBoolCallback callback) {
  bool found = false;
  const char* pref_name = GetBoolPrefNameForApiProperty(property_name.c_str());
  if (pref_name) {
    ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(pref_name,
                                                                    value);
    found = true;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(found)));
}

void ExtensionInfoPrivateAsh::IsTabletModeEnabled(
    IsTabletModeEnabledCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                display::Screen::GetScreen()->InTabletMode()));
}

}  // namespace crosapi
