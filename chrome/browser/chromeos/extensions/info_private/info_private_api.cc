// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private/info_private_api.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "build/config/cuttlefish/buildflags.h"
#include "build/config/squid/buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
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
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/constants/devicetype.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/screen.h"

using ash::NetworkHandler;

namespace {

// Property not found error message.
constexpr std::string_view kPropertyNotFound = "Property '*' does not exist.";

// Key which corresponds to the HWID setting.
constexpr std::string_view kPropertyHWID = "hwid";

// Key which corresponds to the customization ID setting.
constexpr std::string_view kPropertyCustomizationID = "customizationId";

// Key which corresponds to the oem_device_requisition setting.
constexpr std::string_view kPropertyDeviceRequisition = "deviceRequisition";

// Key which corresponds to the isMeetDevice property in JS.
constexpr std::string_view kPropertyMeetDevice = "isMeetDevice";

// Key which corresponds to the isCuttlefishDevice property in JS.
constexpr std::string_view kPropertyCuttlefishDevice = "isCuttlefishDevice";

// Key which corresponds to the isSquidDevice property in JS.
constexpr std::string_view kPropertySquidDevice = "isSquidDevice";

// Key which corresponds to the home provider property.
constexpr std::string_view kPropertyHomeProvider = "homeProvider";

// Key which corresponds to the initial_locale property.
constexpr std::string_view kPropertyInitialLocale = "initialLocale";

// Key which corresponds to the board property in JS.
constexpr std::string_view kPropertyBoard = "board";

// Key which corresponds to the isOwner property in JS.
constexpr std::string_view kPropertyOwner = "isOwner";

// Key which corresponds to the clientId property in JS.
constexpr std::string_view kPropertyClientId = "clientId";

// Key which corresponds to the timezone property in JS.
constexpr std::string_view kPropertySupportedTimezones = "supportedTimezones";

// Key which corresponds to the large cursor A11Y property in JS.
constexpr std::string_view kPropertyLargeCursorEnabled =
    "a11yLargeCursorEnabled";

// Key which corresponds to the sticky keys A11Y property in JS.
constexpr std::string_view kPropertyStickyKeysEnabled = "a11yStickyKeysEnabled";

// Key which corresponds to the spoken feedback A11Y property in JS.
constexpr std::string_view kPropertySpokenFeedbackEnabled =
    "a11ySpokenFeedbackEnabled";

// Key which corresponds to the high contrast mode A11Y property in JS.
constexpr std::string_view kPropertyHighContrastEnabled =
    "a11yHighContrastEnabled";

// Key which corresponds to the screen magnifier A11Y property in JS.
constexpr std::string_view kPropertyScreenMagnifierEnabled =
    "a11yScreenMagnifierEnabled";

// Key which corresponds to the auto click A11Y property in JS.
constexpr std::string_view kPropertyAutoclickEnabled = "a11yAutoClickEnabled";

// Key which corresponds to the auto click A11Y property in JS.
constexpr std::string_view kPropertyVirtualKeyboardEnabled =
    "a11yVirtualKeyboardEnabled";

// Key which corresponds to the caret highlight A11Y property in JS.
constexpr std::string_view kPropertyCaretHighlightEnabled =
    "a11yCaretHighlightEnabled";

// Key which corresponds to the cursor highlight A11Y property in JS.
constexpr std::string_view kPropertyCursorHighlightEnabled =
    "a11yCursorHighlightEnabled";

// Key which corresponds to the focus highlight A11Y property in JS.
constexpr std::string_view kPropertyFocusHighlightEnabled =
    "a11yFocusHighlightEnabled";

// Key which corresponds to the select-to-speak A11Y property in JS.
constexpr std::string_view kPropertySelectToSpeakEnabled =
    "a11ySelectToSpeakEnabled";

// Key which corresponds to the Switch Access A11Y property in JS.
constexpr std::string_view kPropertySwitchAccessEnabled =
    "a11ySwitchAccessEnabled";

// Key which corresponds to the cursor color A11Y property in JS.
constexpr std::string_view kPropertyCursorColorEnabled =
    "a11yCursorColorEnabled";

// Key which corresponds to the docked magnifier property in JS.
constexpr std::string_view kPropertyDockedMagnifierEnabled =
    "a11yDockedMagnifierEnabled";

// Key which corresponds to the send-function-keys property in JS.
constexpr std::string_view kPropertySendFunctionsKeys = "sendFunctionKeys";

// Key which corresponds to the sessionType property in JS.
constexpr std::string_view kPropertySessionType = "sessionType";

// Key which corresponds to the timezone property in JS.
constexpr std::string_view kPropertyTimezone = "timezone";

// Key which corresponds to the "kiosk" value of the SessionType enum in JS.
constexpr std::string_view kSessionTypeKiosk = "kiosk";

// Key which corresponds to the "public session" value of the SessionType enum
// in JS.
constexpr std::string_view kSessionTypePublicSession = "public session";

// Key which corresponds to the "normal" value of the SessionType enum in JS.
constexpr std::string_view kSessionTypeNormal = "normal";

// Key which corresponds to the playStoreStatus property in JS.
constexpr std::string_view kPropertyPlayStoreStatus = "playStoreStatus";

// Key which corresponds to the "not available" value of the PlayStoreStatus
// enum in JS.
constexpr std::string_view kPlayStoreStatusNotAvailable = "not available";

// Key which corresponds to the "available" value of the PlayStoreStatus enum in
// JS.
constexpr std::string_view kPlayStoreStatusAvailable = "available";

// Key which corresponds to the "enabled" value of the PlayStoreStatus enum in
// JS.
constexpr std::string_view kPlayStoreStatusEnabled = "enabled";

// Key which corresponds to the managedDeviceStatus property in JS.
constexpr std::string_view kPropertyManagedDeviceStatus = "managedDeviceStatus";

// Value to which managedDeviceStatus property is set for unmanaged devices.
constexpr std::string_view kManagedDeviceStatusNotManaged = "not managed";

// Value to which managedDeviceStatus property is set for managed devices.
constexpr std::string_view kManagedDeviceStatusManaged = "managed";

// Key which corresponds to the deviceType property in JS.
constexpr std::string_view kPropertyDeviceType = "deviceType";

// Value to which deviceType property is set for Chromebase.
constexpr std::string_view kDeviceTypeChromebase = "chromebase";

// Value to which deviceType property is set for Chromebit.
constexpr std::string_view kDeviceTypeChromebit = "chromebit";

// Value to which deviceType property is set for Chromebook.
constexpr std::string_view kDeviceTypeChromebook = "chromebook";

// Value to which deviceType property is set for Chromebox.
constexpr std::string_view kDeviceTypeChromebox = "chromebox";

// Value to which deviceType property is set when the specific type is unknown.
constexpr std::string_view kDeviceTypeChromedevice = "chromedevice";

// Key which corresponds to the stylusStatus property in JS.
constexpr std::string_view kPropertyStylusStatus = "stylusStatus";

// Value to which stylusStatus property is set when the device does not support
// stylus input.
constexpr std::string_view kStylusStatusUnsupported = "unsupported";

// Value to which stylusStatus property is set when the device supports stylus
// input, but no stylus has been seen before.
constexpr std::string_view kStylusStatusSupported = "supported";

// Value to which stylusStatus property is set when the device has a built-in
// stylus or a stylus has been seen before.
constexpr std::string_view kStylusStatusSeen = "seen";

// Key which corresponds to the assistantStatus property in JS.
constexpr std::string_view kPropertyAssistantStatus = "assistantStatus";

// Value to which assistantStatus property is set when the device supports
// Assistant.
constexpr std::string_view kAssistantStatusSupported = "supported";

constexpr auto kPreferencesMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{kPropertyLargeCursorEnabled,
          ash::prefs::kAccessibilityLargeCursorEnabled},
         {kPropertyStickyKeysEnabled,
          ash::prefs::kAccessibilityStickyKeysEnabled},
         {kPropertySpokenFeedbackEnabled,
          ash::prefs::kAccessibilitySpokenFeedbackEnabled},
         {kPropertyHighContrastEnabled,
          ash::prefs::kAccessibilityHighContrastEnabled},
         {kPropertyScreenMagnifierEnabled,
          ash::prefs::kAccessibilityScreenMagnifierEnabled},
         {kPropertyAutoclickEnabled,
          ash::prefs::kAccessibilityAutoclickEnabled},
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
         {kPropertyCursorColorEnabled,
          ash::prefs::kAccessibilityCursorColorEnabled},
         {kPropertyDockedMagnifierEnabled, ash::prefs::kDockedMagnifierEnabled},
         {kPropertySendFunctionsKeys, ash::prefs::kSendFunctionKeys}});

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

std::optional<std::string_view> GetBoolPrefNameForApiProperty(
    std::string_view api_name) {
  auto it = kPreferencesMap.find(api_name);
  return it != kPreferencesMap.end() ? std::optional(it->second) : std::nullopt;
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

  if (property_name == kPropertyCuttlefishDevice) {
#if BUILDFLAG(PLATFORM_CUTTLEFISH)
    return std::make_unique<base::Value>(true);
#else
    return std::make_unique<base::Value>(false);
#endif
  }

  if (property_name == kPropertySquidDevice) {
    return std::make_unique<base::Value>(
        policy::EnrollmentRequisitionManager::IsSquidDevice());
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
    if (extensions::ExtensionsBrowserClient::Get()
            ->IsRunningInForcedAppMode()) {
      return std::make_unique<base::Value>(kSessionTypeKiosk);
    }
    if (extensions::ExtensionsBrowserClient::Get()
            ->IsLoggedInAsPublicAccount()) {
      return std::make_unique<base::Value>(kSessionTypePublicSession);
    }
    return std::make_unique<base::Value>(kSessionTypeNormal);
  }

  if (property_name == kPropertyPlayStoreStatus) {
    if (arc::IsArcAllowedForProfile(ProfileManager::GetPrimaryUserProfile())) {
      return std::make_unique<base::Value>(kPlayStoreStatusEnabled);
    }
    if (arc::IsArcAvailable()) {
      return std::make_unique<base::Value>(kPlayStoreStatusAvailable);
    }
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

  if (std::optional<std::string_view> pref_name =
          GetBoolPrefNameForApiProperty(property_name)) {
    return std::make_unique<base::Value>(
        ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
            *pref_name));
  }

  DLOG(ERROR) << "Unknown property request: " << property_name;
  return nullptr;
}

base::Value GetSystemProperties(
    const std::vector<std::string>& property_names) {
  base::Value::Dict result;
  for (const std::string& property_name : property_names) {
    std::unique_ptr<base::Value> value = GetValue(property_name);
    if (value) {
      result.Set(property_name,
                 base::Value::FromUniquePtrValue(std::move(value)));
    }
  }
  return base::Value(std::move(result));
}

void SetTimezone(const std::string& value) {
  if (ash::system::PerUserTimezoneEnabled()) {
    ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetString(
        prefs::kUserTimezone, value);
  } else {
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(
            ProfileManager::GetPrimaryUserProfile());
    if (user) {
      ash::system::SetSystemTimezone(user, value);
    }
  }
}

bool SetBool(const std::string& property_name, bool value) {
  std::optional<std::string_view> pref_name =
      GetBoolPrefNameForApiProperty(property_name);
  if (!pref_name) {
    return false;
  }
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(*pref_name,
                                                                  value);
  return true;
}

}  // namespace

namespace extensions {

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() = default;

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() = default;

ExtensionFunction::ResponseAction ChromeosInfoPrivateGetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_list());
  const base::Value::List& list = args()[0].GetList();

  std::vector<std::string> property_names;
  for (const auto& property : list) {
    EXTENSION_FUNCTION_VALIDATE(property.is_string());
    std::string property_name = property.GetString();
    property_names.push_back(std::move(property_name));
  }

  base::Value result = GetSystemProperties(std::move(property_names));
  return RespondNow(WithArguments(std::move(result)));
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() = default;

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() = default;

ExtensionFunction::ResponseAction ChromeosInfoPrivateSetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  param_name_ = args()[0].GetString();

  if (param_name_ == kPropertyTimezone) {
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
    const std::string& param_value = args()[1].GetString();
    SetTimezone(param_value);
    return RespondNow(NoArguments());
  }

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
  bool param_value = args()[1].GetBool();

  if (!SetBool(param_name_, param_value)) {
    return RespondNow(Error(std::string(kPropertyNotFound), param_name_));
  }
  return RespondNow(NoArguments());
}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ChromeosInfoPrivateIsTabletModeEnabledFunction() = default;

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ~ChromeosInfoPrivateIsTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsTabletModeEnabledFunction::Run() {
  return RespondNow(WithArguments(display::Screen::Get()->InTabletMode()));
}

}  // namespace extensions
