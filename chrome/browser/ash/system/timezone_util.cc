// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/timezone_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/timezone/timezone_request.h"
#include "chromeos/ash/components/timezone/timezone_util.h"
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

base::Lock& GetTimezoneBundleLock() {
  static base::NoDestructor<base::Lock> timezone_bundle_lock;
  return *timezone_bundle_lock;
}

const size_t kMaxGeolocationResponseLength = 8;

// Returns an exemplary city in the given timezone.
std::u16string GetExemplarCity(const icu::TimeZone& zone) {
  // These will be leaked at the end.
  static UResourceBundle* zone_bundle = nullptr;
  static UResourceBundle* zone_strings = nullptr;

  UErrorCode status = U_ZERO_ERROR;
  {
    // TODO(jungshik): After upgrading to ICU 4.6, use U_ICUDATA_ZONE in
    // ures_open().
    base::AutoLock lock(GetTimezoneBundleLock());
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
std::u16string GetTimezoneName(const icu::TimeZone& timezone) {
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
  std::string offset_str = base::StringPrintf(
      "UTC%c%d:%02d", offset >= 0 ? '+' : '-', hour_offset, min_remainder);

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
  if (id == icu::UnicodeString(ash::system::kUTCTimezoneName)) {
    name = icu::UnicodeString(
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_NAME_UTC)
            .c_str());
  } else {
    timezone.getDisplayName(dst_offset != 0, icu::TimeZone::LONG, name);
  }
  std::u16string result(l10n_util::GetStringFUTF16(
      IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_TEMPLATE,
      base::ASCIIToUTF16(offset_str), base::i18n::UnicodeStringToString16(name),
      GetExemplarCity(timezone)));
  base::i18n::AdjustStringForLocaleDirection(&result);
  return result;
}

}  // namespace

namespace ash {
namespace system {

std::optional<std::string> GetCountryCodeFromTimezoneIfAvailable(
    const std::string& timezone) {
  // Determine region code from timezone id.
  char region[kMaxGeolocationResponseLength];
  UErrorCode error = U_ZERO_ERROR;
  auto timezone_unicode = icu::UnicodeString::fromUTF8(timezone);
  icu::TimeZone::getRegion(timezone_unicode, region,
                           kMaxGeolocationResponseLength, error);
  // Track failures.
  if (U_FAILURE(error))
    return std::nullopt;

  return base::ToLowerASCII(region);
}

std::u16string GetCurrentTimezoneName() {
  return GetTimezoneName(TimezoneSettings::GetInstance()->GetTimezone());
}

// Creates a list of pairs of each timezone's ID and name.
base::ListValue GetTimezoneList() {
  const auto& timezones = TimezoneSettings::GetInstance()->GetTimezoneList();
  base::ListValue timezone_list;
  for (const auto& timezone : timezones) {
    base::ListValue option;
    option.Append(TimezoneSettings::GetTimezoneID(*timezone));
    option.Append(GetTimezoneName(*timezone));
    timezone_list.Append(std::move(option));
  }
  return timezone_list;
}

bool HasSystemTimezonePolicy() {
  if (!InstallAttributes::Get()->IsEnterpriseManaged())
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

void UpdateSystemTimezone(PrefService& local_state, Profile* profile) {
  if (IsTimezonePrefsManaged(local_state, ash::prefs::kUserTimezone)) {
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
      profile->GetPrefs()->GetString(ash::prefs::kUserTimezone);
  if (user_is_owner) {
    local_state.SetString(ash::prefs::kSigninScreenTimezone, value);
  }

  if (user_manager->GetPrimaryUser() == user &&
      ash::switches::IsPerUserTimezoneEnabled()) {
    SetSystemTimezone(local_state, user, value);
  }
}

void SetTimezoneFromUI(PrefService& local_state,
                       Profile* profile,
                       const std::string& timezone_id) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);

  if (!ash::switches::IsPerUserTimezoneEnabled()) {
    SetSystemTimezone(local_state, user, timezone_id);
    return;
  }

  if (ProfileHelper::IsSigninProfile(profile)) {
    SetSystemAndSigninScreenTimezone(local_state, timezone_id);
    return;
  }

  if (ProfileHelper::IsEphemeralUserProfile(profile)) {
    SetSystemTimezone(local_state, user, timezone_id);
    return;
  }

  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (primary_profile && profile->IsSameOrParent(primary_profile)) {
    profile->GetPrefs()->SetString(ash::prefs::kUserTimezone, timezone_id);
    return;
  }

  // Time zone UI should be blocked for non-primary users.
  NOTREACHED();
}

}  // namespace system
}  // namespace ash
