// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/timezone_resolver_manager.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {
namespace system {

namespace {

// This is the result of several methods calculating configured
// time zone resolve processes.
enum ServiceConfiguration {
  UNSPECIFIED = 0,   // Try another configuration source.
  SHOULD_START = 1,  // This source requires service Start.
  SHOULD_STOP = 2,   // This source requires service Stop.
};

// Starts or stops TimezoneResolver if required by
// SystemTimezoneAutomaticDetectionPolicy.
// Returns SHOULD_* if timezone resolver status is controlled by this policy.
ServiceConfiguration GetServiceConfigurationFromAutomaticDetectionPolicy() {
  PrefService* local_state = g_browser_process->local_state();
  const bool is_managed = local_state->IsManagedPreference(
      ::prefs::kSystemTimezoneAutomaticDetectionPolicy);
  if (!is_managed)
    return UNSPECIFIED;

  int policy_value =
      local_state->GetInteger(::prefs::kSystemTimezoneAutomaticDetectionPolicy);

  switch (policy_value) {
    case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
      return UNSPECIFIED;
    case enterprise_management::SystemTimezoneProto::DISABLED:
      return SHOULD_STOP;
    case enterprise_management::SystemTimezoneProto::IP_ONLY:
      return SHOULD_START;
    case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
      return SHOULD_START;
    case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
      return SHOULD_START;
  }
  // Default for unknown policy value.
  NOTREACHED_IN_MIGRATION() << "Unrecognized policy value: " << policy_value;
  return SHOULD_STOP;
}

// Stops TimezoneResolver if SystemTimezonePolicy is applied.
// Returns SHOULD_* if timezone resolver status is controlled by this policy.
ServiceConfiguration GetServiceConfigurationFromSystemTimezonePolicy() {
  if (!HasSystemTimezonePolicy())
    return UNSPECIFIED;

  return SHOULD_STOP;
}

// Starts or stops TimezoneResolver if required by policy.
// Returns true if timezone resolver status is controlled by policy.
ServiceConfiguration GetServiceConfigurationFromPolicy() {
  ServiceConfiguration result =
      GetServiceConfigurationFromSystemTimezonePolicy();

  if (result != UNSPECIFIED)
    return result;

  result = GetServiceConfigurationFromAutomaticDetectionPolicy();
  return result;
}

// Returns service configuration for the user.
ServiceConfiguration GetServiceConfigurationFromUserPrefs(
    const PrefService* user_prefs) {
  return TimeZoneResolverManager::TimeZoneResolveMethodFromInt(
             user_prefs->GetInteger(
                 ::prefs::kResolveTimezoneByGeolocationMethod)) ==
                 TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED
             ? SHOULD_STOP
             : SHOULD_START;
}

// Returns service configuration for the signin screen.
ServiceConfiguration GetServiceConfigurationForSigninScreen() {
  using AccessLevel = GeolocationAccessLevel;

  const AccessLevel device_geolocation_permission =
      static_cast<AccessLevel>(g_browser_process->local_state()->GetInteger(
          prefs::kDeviceGeolocationAllowed));
  if (device_geolocation_permission == AccessLevel::kDisallowed) {
    return SHOULD_STOP;
  }

  const PrefService::Preference* device_pref =
      g_browser_process->local_state()->FindPreference(
          ::prefs::kResolveDeviceTimezoneByGeolocationMethod);
  if (!device_pref || device_pref->IsDefaultValue()) {
    // CfM devices default to static timezone.
    bool keyboard_driven_oobe =
        system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
    return keyboard_driven_oobe ? SHOULD_STOP : SHOULD_START;
  }

  // Do not start resolver if we are inside active user session.
  // If user preferences permit, it will be started on preferences
  // initialization.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser))
    return SHOULD_STOP;

  return TimeZoneResolverManager::TimeZoneResolveMethodFromInt(
             device_pref->GetValue()->GetInt()) ==
                 TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED
             ? SHOULD_STOP
             : SHOULD_START;
}

}  // anonymous namespace.

TimeZoneResolverManager::TimeZoneResolverManager(
    SimpleGeolocationProvider* geolocation_provider,
    session_manager::SessionManager* session_manager)
    : geolocation_provider_(geolocation_provider) {
  switch (g_browser_process->local_state()->GetInitializationStatus()) {
    case PrefService::INITIALIZATION_STATUS_SUCCESS:
    case PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE:
      local_state_initialized_ = true;
      break;
    default:
      local_state_initialized_ = false;
  }
  g_browser_process->local_state()->AddPrefInitObserver(
      base::BindOnce(&TimeZoneResolverManager::OnLocalStateInitialized,
                     weak_factory_.GetWeakPtr()));

  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      ::prefs::kSystemTimezoneAutomaticDetectionPolicy,
      base::BindRepeating(&TimeZoneResolverManager::UpdateTimezoneResolver,
                          base::Unretained(this)));

  geolocation_provider_->AddObserver(this);
  session_observation_.Observe(session_manager);
}

TimeZoneResolverManager::~TimeZoneResolverManager() {
  geolocation_provider_->RemoveObserver(this);
  geolocation_provider_ = nullptr;
}

void TimeZoneResolverManager::SetPrimaryUserPrefs(PrefService* pref_service) {
  primary_user_prefs_ = pref_service;
}

bool TimeZoneResolverManager::ShouldSendWiFiGeolocationData() const {
  // Managed user case, check cloud policies for automatic time zone.
  if (IsTimeZoneResolutionPolicyControlled()) {
    switch (GetEffectiveAutomaticTimezoneManagementSetting()) {
      case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
        return true;
      case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
        return true;
      case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
        // Same as regular user flow, continue processing below.
        break;
      default:
        return false;
    }
  }

  // Regular user case (also USERS_DECIDE case for managed).
  // primary_user_prefs_ indicates if the user has signed in or not.
  // Precise location is disabled on log-in screen.
  if (!primary_user_prefs_) {
    return false;
  }

  // User is logged in at this point.
  // Check that user location permission is granted for system services.
  if (static_cast<GeolocationAccessLevel>(primary_user_prefs_->GetInteger(
          ash::prefs::kUserGeolocationAccessLevel)) ==
      GeolocationAccessLevel::kDisallowed) {
    return false;
  }

  // Automatic time zone setting is a user configurable option, applying
  // the primary user's choice to the entire session.
  switch (GetEffectiveUserTimeZoneResolveMethod(primary_user_prefs_,
                                                /*check_policy=*/false)) {
    case TimeZoneResolveMethod::SEND_WIFI_ACCESS_POINTS:
      return true;
    case TimeZoneResolveMethod::SEND_ALL_LOCATION_INFO:
      return true;
    default:
      return false;
  }
}

bool TimeZoneResolverManager::ShouldSendCellularGeolocationData() const {
  // Managed user case, check cloud policies for automatic time zone.
  if (IsTimeZoneResolutionPolicyControlled()) {
    switch (GetEffectiveAutomaticTimezoneManagementSetting()) {
      case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
        return true;
      case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
        // Same as regular user flow, continue processing below.
        break;
      default:
        return false;
    }
  }

  // Regular user case (also USERS_DECIDE case for managed).
  // primary_user_prefs_ indicates if the user has signed in or not.
  // Precise location is disabled on log-in screen.
  if (!primary_user_prefs_) {
    return false;
  }

  // User is logged in at this point.
  // Check that user location permission is granted for system services.
  if (static_cast<GeolocationAccessLevel>(primary_user_prefs_->GetInteger(
          ash::prefs::kUserGeolocationAccessLevel)) ==
      GeolocationAccessLevel::kDisallowed) {
    return false;
  }

  // Automatic time zone setting is a user configurable option, applying
  // the primary user's choice to the entire session.
  return GetEffectiveUserTimeZoneResolveMethod(primary_user_prefs_,
                                               /*check_policy=*/false) ==
         TimeZoneResolveMethod::SEND_ALL_LOCATION_INFO;
}

// static
int TimeZoneResolverManager::GetEffectiveAutomaticTimezoneManagementSetting() {
  // Regular users choose automatic time zone method themselves.
  if (!IsTimeZoneResolutionPolicyControlled()) {
    return enterprise_management::SystemTimezoneProto::USERS_DECIDE;
  }

  // Static time zone policy overrides automatic.
  if (HasSystemTimezonePolicy()) {
    return enterprise_management::SystemTimezoneProto::DISABLED;
  }

  int policy_value = g_browser_process->local_state()->GetInteger(
      ::prefs::kSystemTimezoneAutomaticDetectionPolicy);
  DCHECK(policy_value <= enterprise_management::SystemTimezoneProto::
                             AutomaticTimezoneDetectionType_MAX);

  return policy_value;
}

void TimeZoneResolverManager::OnUserProfileLoaded(const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  system::UpdateSystemTimezone(profile);

  auto* user_manager = user_manager::UserManager::Get();
  const auto* user = user_manager->FindUser(account_id);
  if (!user) {
    return;
  }

  // In Multi-Profile mode only primary user settings are in effect.
  if (user != user_manager->GetPrimaryUser()) {
    return;
  }

  if (!user_manager->IsUserLoggedIn()) {
    return;
  }

  // Timezone auto refresh is disabled for Guest and OffTheRecord
  // users, but enabled for Kiosk mode.
  if (user_manager->IsLoggedInAsGuest() || profile->IsOffTheRecord()) {
    GetResolver()->Stop();
    return;
  }
  UpdateTimezoneResolver();
}

void TimeZoneResolverManager::UpdateTimezoneResolver() {
  initialized_ = true;
  TimeZoneResolver* resolver = GetResolver();
  // Local state becomes initialized when policy data is loaded,
  // and we need policies to decide whether resolver can be started.
  if (!local_state_initialized_) {
    resolver->Stop();
    return;
  }
  if (TimeZoneResolverShouldBeRunning())
    resolver->Start();
  else
    resolver->Stop();

  // Observers must be notified whenever UpdateTimezoneResolver() is called.
  // This allows observers to listen for all relevant prefs updates.
  for (Observer& observer : observers_)
    observer.OnTimeZoneResolverUpdated();
}

void TimeZoneResolverManager::OnGeolocationPermissionChanged(bool enabled) {
  // New permission state will be retrieved from `geolocation_provider_`.
  UpdateTimezoneResolver();
}

void TimeZoneResolverManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TimeZoneResolverManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TimeZoneResolverManager::ShouldApplyResolvedTimezone() {
  return TimeZoneResolverShouldBeRunning();
}

bool TimeZoneResolverManager::TimeZoneResolverShouldBeRunning() {
  // System geolocation permission is required for automatic timezone
  // resolution.
  if (!geolocation_provider_->IsGeolocationUsageAllowedForSystem()) {
    return false;
  }

  // Once the permission is granted, it's all up to the time zone
  // configuration data.
  return TimeZoneResolverAllowedByTimeZoneConfigData();
}

bool TimeZoneResolverManager::TimeZoneResolverAllowedByTimeZoneConfigData() {
  ServiceConfiguration result = GetServiceConfigurationFromPolicy();

  if (result == UNSPECIFIED) {
    if (primary_user_prefs_) {
      result = GetServiceConfigurationFromUserPrefs(primary_user_prefs_);
    } else {
      // We are on a signin page.
      result = GetServiceConfigurationForSigninScreen();
    }
  }
  return result == SHOULD_START;
}

ash::TimeZoneResolver* TimeZoneResolverManager::GetResolver() {
  if (!timezone_resolver_.get()) {
    timezone_resolver_ = std::make_unique<ash::TimeZoneResolver>(
        this, geolocation_provider_,
        g_browser_process->shared_url_loader_factory(),
        base::BindRepeating(&ash::system::ApplyTimeZone),
        base::BindRepeating(&ash::DelayNetworkCall),
        g_browser_process->local_state());
  }
  return timezone_resolver_.get();
}

void TimeZoneResolverManager::OnLocalStateInitialized(bool initialized) {
  local_state_initialized_ = initialized;
  if (initialized_)
    UpdateTimezoneResolver();
}

// static
TimeZoneResolverManager::TimeZoneResolveMethod
TimeZoneResolverManager::TimeZoneResolveMethodFromInt(int value) {
  if (value < 0 ||
      value >= static_cast<int>(TimeZoneResolveMethod::METHODS_NUMBER)) {
    return TimeZoneResolveMethod::DISABLED;
  }

  const TimeZoneResolveMethod method =
      static_cast<TimeZoneResolveMethod>(value);

  if (FineGrainedTimeZoneDetectionEnabled())
    return method;

  if (method == TimeZoneResolveMethod::DISABLED)
    return TimeZoneResolveMethod::DISABLED;

  return TimeZoneResolveMethod::IP_ONLY;
}

// static
TimeZoneResolverManager::TimeZoneResolveMethod
TimeZoneResolverManager::GetEffectiveUserTimeZoneResolveMethod(
    const PrefService* user_prefs,
    bool check_policy) {
  if (check_policy) {
    int policy_value = GetEffectiveAutomaticTimezoneManagementSetting();
    switch (policy_value) {
      case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
        // Follow user preference.
        break;
      case enterprise_management::SystemTimezoneProto::DISABLED:
        return TimeZoneResolveMethod::DISABLED;
      case enterprise_management::SystemTimezoneProto::IP_ONLY:
        return TimeZoneResolveMethod::IP_ONLY;
      case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
        return TimeZoneResolveMethod::SEND_WIFI_ACCESS_POINTS;
      case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
        return TimeZoneResolveMethod::SEND_ALL_LOCATION_INFO;
      default:
        NOTREACHED_IN_MIGRATION();
        return TimeZoneResolveMethod::DISABLED;
    }
  }
  if (user_prefs->GetBoolean(
          ::prefs::kResolveTimezoneByGeolocationMigratedToMethod)) {
    return TimeZoneResolveMethodFromInt(
        user_prefs->GetInteger(::prefs::kResolveTimezoneByGeolocationMethod));
  }
  return TimeZoneResolveMethod::IP_ONLY;
}

// static
bool TimeZoneResolverManager::IsTimeZoneResolutionPolicyControlled() {
  return GetServiceConfigurationFromPolicy() != UNSPECIFIED;
}

// static
bool TimeZoneResolverManager::IfServiceShouldBeRunningForSigninScreen() {
  return GetServiceConfigurationForSigninScreen() == SHOULD_START;
}

}  // namespace system
}  // namespace ash
