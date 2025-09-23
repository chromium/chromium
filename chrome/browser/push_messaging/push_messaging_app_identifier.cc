// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"

#include <string.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

std::string FromTimeToString(base::Time time) {
  DCHECK(!time.is_null());
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMilliseconds());
}

bool FromStringToTime(const std::string& time_string,
                      std::optional<base::Time>* time) {
  DCHECK(!time_string.empty());
  int64_t milliseconds;
  if (base::StringToInt64(time_string, &milliseconds) && milliseconds > 0) {
    *time = std::make_optional(base::Time::FromDeltaSinceWindowsEpoch(
        base::Milliseconds(milliseconds)));
    return true;
  }
  return false;
}

std::string MakePrefValue(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::optional<base::Time>& expiration_time = std::nullopt) {
  std::string result = origin.spec() + push_messaging::kPrefValueSeparator +
                       base::NumberToString(service_worker_registration_id);
  if (expiration_time)
    result += push_messaging::kPrefValueSeparator +
              FromTimeToString(*expiration_time);
  return result;
}

bool DisassemblePrefValue(const std::string& pref_value,
                          GURL* origin,
                          int64_t* service_worker_registration_id,
                          std::optional<base::Time>* expiration_time) {
  std::vector<std::string> parts = base::SplitString(
      pref_value, std::string(1, push_messaging::kPrefValueSeparator),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (parts.size() < 2 || parts.size() > 3)
    return false;

  if (!base::StringToInt64(parts[1], service_worker_registration_id))
    return false;

  *origin = GURL(parts[0]);
  if (!origin->is_valid())
    return false;

  if (parts.size() == 3)
    return FromStringToTime(parts[2], expiration_time);

  return true;
}

}  // namespace

using AppIdentifier = ::push_messaging::AppIdentifier;

// static
void PushMessagingAppIdentifier::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(johnme): If push becomes enabled in incognito, be careful that this
  // pref is read from the right profile, as prefs defined in a regular profile
  // are visible in the corresponding incognito profile unless overridden.
  // TODO(johnme): Make sure this pref doesn't get out of sync after crashes.
  registry->RegisterDictionaryPref(prefs::kPushMessagingAppIdentifierMap);
}

// static
AppIdentifier PushMessagingAppIdentifier::FindByAppId(
    Profile* profile,
    const std::string& app_id) {
  if (!base::StartsWith(app_id, push_messaging::kAppIdentifierPrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return AppIdentifier::GenerateInvalid();
  }

  // Since we now know this is a Push Messaging app_id, check the case hasn't
  // been mangled (crbug.com/461867).
  DCHECK_EQ(push_messaging::kAppIdentifierPrefix,
            app_id.substr(0, push_messaging::kPrefixLength));
  DCHECK_GE(app_id.size(),
            push_messaging::kPrefixLength + push_messaging::kGuidLength);
  DCHECK_EQ(app_id.substr(app_id.size() - push_messaging::kGuidLength),
            base::ToUpperASCII(
                app_id.substr(app_id.size() - push_messaging::kGuidLength)));

  const base::Value::Dict& map =
      profile->GetPrefs()->GetDict(prefs::kPushMessagingAppIdentifierMap);

  const std::string* map_value = map.FindString(app_id);

  if (!map_value || map_value->empty())
    return AppIdentifier::GenerateInvalid();

  GURL origin;
  int64_t service_worker_registration_id;
  std::optional<base::Time> expiration_time;
  // Try disassemble the pref value, return an invalid app identifier if the
  // pref value is corrupted
  if (!DisassemblePrefValue(*map_value, &origin,
                            &service_worker_registration_id,
                            &expiration_time)) {
    NOTREACHED();
  }

  return AppIdentifier::GenerateDirect(
      app_id, origin, service_worker_registration_id, expiration_time);
}

// static
AppIdentifier PushMessagingAppIdentifier::FindByServiceWorker(
    Profile* profile,
    const GURL& origin,
    int64_t service_worker_registration_id) {
  const std::string base_pref_value =
      MakePrefValue(origin, service_worker_registration_id);

  const base::Value::Dict& map =
      profile->GetPrefs()->GetDict(prefs::kPushMessagingAppIdentifierMap);
  for (auto entry : map) {
    if (entry.second.is_string() &&
        base::StartsWith(entry.second.GetString(), base_pref_value,
                         base::CompareCase::SENSITIVE)) {
      return FindByAppId(profile, entry.first);
    }
  }
  return AppIdentifier::GenerateInvalid();
}

// static
std::vector<AppIdentifier> PushMessagingAppIdentifier::GetAll(
    Profile* profile) {
  std::vector<AppIdentifier> result;

  const base::Value::Dict& map =
      profile->GetPrefs()->GetDict(prefs::kPushMessagingAppIdentifierMap);
  for (auto entry : map) {
    result.push_back(FindByAppId(profile, entry.first));
  }

  return result;
}

// static
void PushMessagingAppIdentifier::DeleteAllFromPrefs(Profile* profile) {
  profile->GetPrefs()->SetDict(prefs::kPushMessagingAppIdentifierMap,
                               base::Value::Dict());
}

// static
size_t PushMessagingAppIdentifier::GetCount(Profile* profile) {
  return profile->GetPrefs()
      ->GetDict(prefs::kPushMessagingAppIdentifierMap)
      .size();
}

// static
void PushMessagingAppIdentifier::PersistToPrefs(const AppIdentifier& id,
                                                Profile* profile) {
  id.DCheckValid();
  CHECK(!id.is_null());

  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::Value::Dict& map = update.Get();

  // Delete any stale entry with the same origin and Service Worker
  // registration id (hence we ensure there is a 1:1 not 1:many mapping).
  AppIdentifier old = FindByServiceWorker(profile, id.origin(),
                                          id.service_worker_registration_id());
  if (!old.is_null())
    map.Remove(old.app_id());

  map.Set(id.app_id(),
          MakePrefValue(id.origin(), id.service_worker_registration_id(),
                        id.expiration_time()));
}

// static
void PushMessagingAppIdentifier::DeleteFromPrefs(const AppIdentifier& id,
                                                 Profile* profile) {
  id.DCheckValid();

  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::Value::Dict& map = update.Get();
  map.Remove(id.app_id());
}
