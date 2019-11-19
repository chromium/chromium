// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/timezone/timezone_request.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/common/unicode/ures.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct UResClose {
  inline void operator() (UResourceBundle* b) const {
    ures_close(b);
  }
};

base::LazyInstance<base::Lock>::Leaky g_timezone_bundle_lock =
    LAZY_INSTANCE_INITIALIZER;

// Returns an exemplary city in the given timezone.
base::string16 GetExemplarCity(const icu::TimeZone& zone) {
  // These will be leaked at the end.
  static UResourceBundle* zone_bundle = nullptr;
  static UResourceBundle* zone_strings = nullptr;

  UErrorCode status = U_ZERO_ERROR;
  {
    // TODO(jungshik): After upgrading to ICU 4.6, use U_ICUDATA_ZONE in
    // ures_open().
    base::AutoLock lock(g_timezone_bundle_lock.Get());
    if (!zone_bundle)
      zone_bundle = ures_open(nullptr, uloc_getDefault(), &status);

    if (!zone_strings) {
      zone_strings =
          ures_getByKey(zone_bundle, "zone_strings", nullptr, &status);
    }
  }

  icu::UnicodeString zone_id;
  zone.getID(zone_id);
  std::string zone_id_str;
  zone_id.toUTF8String(zone_id_str);

  // Resource keys for timezones use ':' in place of '/'.
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, "/", ":");
  std::unique_ptr<UResourceBundle, UResClose> zone_item(
      ures_getByKey(zone_strings, zone_id_str.c_str(), nullptr, &status));
  icu::UnicodeString city;
  if (!U_FAILURE(status)) {
    city = icu::ures_getUnicodeStringByKey(zone_item.get(), "ec", &status);
    if (U_SUCCESS(status))
      return base::i18n::UnicodeStringToString16(city);
  }

  // Fallback case in case of failure.
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, ":", "/");
  // Take the last component of a timezone id (e.g. 'Baz' in 'Foo/Bar/Baz').
  // Depending on timezones, keeping all but the 1st component
  // (e.g. Bar/Baz) may be better, but our current list does not have
  // any timezone for which that's the case.
  std::string::size_type slash_pos = zone_id_str.rfind('/');
  if (slash_pos != std::string::npos && slash_pos < zone_id_str.size())
    zone_id_str.erase(0, slash_pos + 1);
  // zone id has '_' in place of ' '.
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, "_", " ");
  return base::ASCIIToUTF16(zone_id_str);
}

// Gets the given timezone's name for visualization.
base::string16 GetTimezoneName(const icu::TimeZone& timezone) {
  // Instead of using the raw_offset, use the offset in effect now.
  // For instance, US Pacific Time, the offset shown will be -7 in summer
  // while it'll be -8 in winter.
  int raw_offset, dst_offset;
  UDate now = icu::Calendar::getNow();
  UErrorCode status = U_ZERO_ERROR;
  timezone.getOffset(now, false, raw_offset, dst_offset, status);
  DCHECK(U_SUCCESS(status));
  int offset = raw_offset + dst_offset;
  // |offset| is in msec.
  int minute_offset = std::abs(offset) / 60000;
  int hour_offset = minute_offset / 60;
  int min_remainder = minute_offset % 60;
  // Some timezones have a non-integral hour offset. So, we need to use hh:mm
  // form.
  std::string  offset_str = base::StringPrintf(offset >= 0 ?
      "UTC+%d:%02d" : "UTC-%d:%02d", hour_offset, min_remainder);

  // TODO(jungshik): When coming up with a better list of timezones, we also
  // have to come up with better 'display' names. One possibility is to list
  // multiple cities (e.g. "Los Angeles, Vancouver .." in the order of
  // the population of a country the city belongs to.).
  // We can also think of using LONG_GENERIC or LOCATION once we upgrade
  // to ICU 4.6.
  // In the meantime, we use "LONG" name with "Exemplar City" to distinguish
  // multiple timezones with the same "LONG" name but with different
  // rules (e.g. US Mountain Time in Denver vs Phoenix).
  icu::UnicodeString id;
  icu::UnicodeString name;
  timezone.getID(id);
  if (id == icu::UnicodeString(chromeos::system::kUTCTimezoneName)) {
    name = icu::UnicodeString(
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_NAME_UTC)
            .c_str());
  } else {
    timezone.getDisplayName(dst_offset != 0, icu::TimeZone::LONG, name);
  }
  base::string16 result(l10n_util::GetStringFUTF16(
      IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_TEMPLATE,
      base::ASCIIToUTF16(offset_str), base::i18n::UnicodeStringToString16(name),
      GetExemplarCity(timezone)));
  base::i18n::AdjustStringForLocaleDirection(&result);
  return result;
}

bool CanSetSystemTimezoneFromManagedGuestSession() {
  const int automatic_detection_policy =
      g_browser_process->local_state()->GetInteger(
          prefs::kSystemTimezoneAutomaticDetectionPolicy);

  return (automatic_detection_policy ==
          enterprise_management::SystemTimezoneProto::DISABLED) ||
         (automatic_detection_policy ==
          enterprise_management::SystemTimezoneProto::USERS_DECIDE);
}

// Returns true if the given user is allowed to set the system timezone - that
// is, the single timezone at TimezoneSettings::GetInstance()->GetTimezone(),
// which is also stored in a file at /var/lib/timezone/localtime.
bool CanSetSystemTimezone(const user_manager::User* user) {
  if (!user->is_logged_in())
    return false;

  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return true;

    case user_manager::USER_TYPE_GUEST:
      return false;

    case user_manager::USER_TYPE_CHILD:
      return base::FeatureList::IsEnabled(
          features::kParentAccessCodeForTimeChange);

    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return CanSetSystemTimezoneFromManagedGuestSession();

    case user_manager::NUM_USER_TYPES:
      NOTREACHED();

      // No default case means the compiler makes sure we handle new types.
  }
  NOTREACHED();
  return false;
}

}  // namespace

namespace chromeos {
namespace system {

base::string16 GetCurrentTimezoneName() {
  return GetTimezoneName(TimezoneSettings::GetInstance()->GetTimezone());
}

// Creates a list of pairs of each timezone's ID and name.
std::unique_ptr<base::ListValue> GetTimezoneList() {
  const auto& timezones = TimezoneSettings::GetInstance()->GetTimezoneList();
  auto timezone_list = std::make_unique<base::ListValue>();
  for (const auto& timezone : timezones) {
    auto option = std::make_unique<base::ListValue>();
    option->AppendString(TimezoneSettings::GetTimezoneID(*timezone));
    option->AppendString(GetTimezoneName(*timezone));
    timezone_list->Append(std::move(option));
  }
  return timezone_list;
}

bool HasSystemTimezonePolicy() {
  if (!chromeos::InstallAttributes::Get()->IsEnterpriseManaged())
    return false;

  std::string policy_timezone;
  if (CrosSettings::Get()->GetString(kSystemTimezonePolicy, &policy_timezone) &&
      !policy_timezone.empty()) {
    VLOG(1) << "Refresh TimeZone: TimeZone settings are overridden"
            << " by DevicePolicy.";
    return true;
  }
  return false;
}

bool IsTimezonePrefsManaged(const std::string& pref_name) {
  DCHECK(pref_name == chromeos::kSystemTimezone ||
         pref_name == prefs::kUserTimezone ||
         pref_name == prefs::kResolveTimezoneByGeolocationMethod);

  std::string policy_timezone;
  if (CrosSettings::Get()->GetString(kSystemTimezonePolicy, &policy_timezone) &&
      !policy_timezone.empty()) {
    return true;
  }

  // System time zone preference is managed only if kSystemTimezonePolicy
  // present, which we checked above.
  //
  // kSystemTimezoneAutomaticDetectionPolicy (see below) controls only user
  // time zone preference, and user time zone resolve preference.
  if (pref_name == chromeos::kSystemTimezone)
    return false;

  const PrefService* local_state = g_browser_process->local_state();
  if (!local_state->IsManagedPreference(
          prefs::kSystemTimezoneAutomaticDetectionPolicy)) {
    return false;
  }

  int resolve_policy_value =
      local_state->GetInteger(prefs::kSystemTimezoneAutomaticDetectionPolicy);

  switch (resolve_policy_value) {
    case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
      return false;
    case enterprise_management::SystemTimezoneProto::DISABLED:
      // This only disables resolving.
      return pref_name == prefs::kResolveTimezoneByGeolocationMethod;
    case enterprise_management::SystemTimezoneProto::IP_ONLY:
    case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
    case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
      return true;
  }
  // Default for unknown policy value.
  NOTREACHED() << "Unrecognized policy value: " << resolve_policy_value;
  return true;
}

void ApplyTimeZone(const TimeZoneResponseData* timezone) {
  if (!g_browser_process->platform_part()
           ->GetTimezoneResolverManager()
           ->ShouldApplyResolvedTimezone()) {
    return;
  }

  if (timezone->timeZoneId.empty())
    return;

  VLOG(1) << "Refresh TimeZone: setting timezone to '" << timezone->timeZoneId
          << "'";

  if (PerUserTimezoneEnabled()) {
    const user_manager::UserManager* user_manager =
        user_manager::UserManager::Get();
    const user_manager::User* primary_user = user_manager->GetPrimaryUser();

    if (primary_user) {
      Profile* profile = ProfileHelper::Get()->GetProfileByUser(primary_user);
      // profile can be NULL only if user has logged in, but profile has not
      // been initialized yet. Ignore delayed time zone update until user
      // preferences are initialized.
      if (!profile)
        return;

      profile->GetPrefs()->SetString(prefs::kUserTimezone,
                                     timezone->timeZoneId);
      // For non-enterprise device, chromeos::Preferences::ApplyPreferences()
      // will automatically change system timezone because user is primary.
      // But it may not happen for enterprise device, as policy may prevent
      // user from changing device time zone manually.
      // That is the reason we always update system time zone here.
      TimezoneSettings::GetInstance()->SetTimezoneFromID(
          base::UTF8ToUTF16(timezone->timeZoneId));
    } else {
      SetSystemAndSigninScreenTimezone(timezone->timeZoneId);
    }
  } else {
    TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone->timeZoneId));
  }
}

void UpdateSystemTimezone(Profile* profile) {
  if (IsTimezonePrefsManaged(prefs::kUserTimezone)) {
    VLOG(1) << "Ignoring user timezone change, because timezone is enterprise "
               "managed.";
    return;
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);

  const AccountId owner(user_manager->GetOwnerAccountId());
  const bool user_is_owner =
      owner.is_valid() && (owner == user->GetAccountId());

  const std::string value =
      profile->GetPrefs()->GetString(prefs::kUserTimezone);
  if (user_is_owner) {
    g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                                value);
  }

  if (user_manager->GetPrimaryUser() == user && PerUserTimezoneEnabled())
    SetSystemTimezone(user, value);
}

bool SetSystemTimezone(const user_manager::User* user,
                       const std::string& timezone) {
  DCHECK(user);
  if (!CanSetSystemTimezone(user))
    return false;
  TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16(timezone));
  return true;
}

void SetSystemAndSigninScreenTimezone(const std::string& timezone) {
  if (timezone.empty())
    return;

  g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                              timezone);

  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  if (current_timezone_id != timezone) {
    TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone));
  }
}

bool PerUserTimezoneEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisablePerUserTimezone);
}

void SetTimezoneFromUI(Profile* profile, const std::string& timezone_id) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);

  if (!PerUserTimezoneEnabled()) {
    SetSystemTimezone(user, timezone_id);
    return;
  }

  if (ProfileHelper::IsSigninProfile(profile)) {
    SetSystemAndSigninScreenTimezone(timezone_id);
    return;
  }

  if (ProfileHelper::IsEphemeralUserProfile(profile)) {
    SetSystemTimezone(user, timezone_id);
    return;
  }

  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (primary_profile && profile->IsSameProfile(primary_profile)) {
    profile->GetPrefs()->SetString(prefs::kUserTimezone, timezone_id);
    return;
  }

  // Time zone UI should be blocked for non-primary users.
  NOTREACHED();
}

bool FineGrainedTimeZoneDetectionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableFineGrainedTimeZoneDetection);
}

}  // namespace system
}  // namespace chromeos
