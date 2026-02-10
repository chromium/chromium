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
  // been mangled (crbug.com/40407250).
  DCHECK_EQ(push_messaging::kAppIdentifierPrefix,
            app_id.substr(0, push_messaging::kPrefixLength));
  DCHECK_GE(app_id.size(),
            push_messaging::kPrefixLength + push_messaging::kGuidLength);
  DCHECK_EQ(app_id.substr(app_id.size() - push_messaging::kGuidLength),
            base::ToUpperASCII(
                app_id.substr(app_id.size() - push_messaging::kGuidLength)));

  const base::DictValue& map =
      profile->GetPrefs()->GetDict(prefs::kPushMessagingAppIdentifierMap);

  const std::string* map_value = map.FindString(app_id);

  if (!map_value || map_value->empty())
    return AppIdentifier::GenerateInvalid();

  auto result = AppIdentifier::FromPrefValue(app_id, *map_value);
  CHECK(result);
  return *result;
}

// static
AppIdentifier PushMessagingAppIdentifier::FindByServiceWorker(
    Profile* profile,
    const GURL& origin,
    int64_t service_worker_registration_id) {
  const std::string base_pref_value =
      AppIdentifier::Generate(origin, service_worker_registration_id)
          .ToPrefValue();

  const base::DictValue& map =
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

  const base::DictValue& map =
      profile->GetPrefs()->GetDict(prefs::kPushMessagingAppIdentifierMap);
  for (auto entry : map) {
    result.push_back(FindByAppId(profile, entry.first));
  }

  return result;
}

// static
void PushMessagingAppIdentifier::DeleteAllFromPrefs(Profile* profile) {
  profile->GetPrefs()->SetDict(prefs::kPushMessagingAppIdentifierMap,
                               base::DictValue());
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
  base::DictValue& map = update.Get();

  // Delete any stale entry with the same origin and Service Worker
  // registration id (hence we ensure there is a 1:1 not 1:many mapping).
  AppIdentifier old = FindByServiceWorker(profile, id.origin(),
                                          id.service_worker_registration_id());
  if (!old.is_null())
    map.Remove(old.app_id());

  map.Set(id.app_id(), id.ToPrefValue());
}

// static
void PushMessagingAppIdentifier::DeleteFromPrefs(const AppIdentifier& id,
                                                 Profile* profile) {
  id.DCheckValid();

  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictValue& map = update.Get();
  map.Remove(id.app_id());
}
