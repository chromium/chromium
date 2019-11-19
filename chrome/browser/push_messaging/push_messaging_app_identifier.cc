// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"

#include <string.h>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

const char kPushMessagingAppIdentifierPrefix[] = "wp:";
const char kInstanceIDGuidSuffix[] = "-V2";

namespace {

// sizeof is strlen + 1 since it's null-terminated.
const size_t kPrefixLength = sizeof(kPushMessagingAppIdentifierPrefix) - 1;
const size_t kGuidSuffixLength = sizeof(kInstanceIDGuidSuffix) - 1;

// Ok to use '#' as separator since only the origin of the url is used.
const char kOriginSWRIdSeparator = '#';
const size_t kGuidLength = 36;  // "%08X-%04X-%04X-%04X-%012llX"

std::string MakePrefValue(const GURL& origin,
                          int64_t service_worker_registration_id) {
  return origin.spec() + kOriginSWRIdSeparator +
         base::NumberToString(service_worker_registration_id);
}

bool GetOriginAndSWRFromPrefValue(const std::string& pref_value,
                                  GURL* origin,
                                  int64_t* service_worker_registration_id) {
  std::vector<std::string> parts =
      base::SplitString(pref_value, std::string(1, kOriginSWRIdSeparator),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  if (!base::StringToInt64(parts[1], service_worker_registration_id))
    return false;

  *origin = GURL(parts[0]);
  return origin->is_valid();
}

}  // namespace

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
bool PushMessagingAppIdentifier::UseInstanceID(const std::string& app_id) {
  return base::EndsWith(app_id, kInstanceIDGuidSuffix,
                        base::CompareCase::SENSITIVE);
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::Generate(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  // All new push subscriptions use Instance ID tokens.
  return GenerateInternal(origin, service_worker_registration_id,
                          true /* use_instance_id */);
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::LegacyGenerateForTesting(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  return GenerateInternal(origin, service_worker_registration_id,
                          false /* use_instance_id */);
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::GenerateInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    bool use_instance_id) {
  // Use uppercase GUID for consistency with GUIDs Push has already sent to GCM.
  // Also allows detecting case mangling; see code commented "crbug.com/461867".
  std::string guid = base::ToUpperASCII(base::GenerateGUID());
  if (use_instance_id) {
    guid.replace(guid.size() - kGuidSuffixLength, kGuidSuffixLength,
                 kInstanceIDGuidSuffix);
  }
  CHECK(!guid.empty());
  std::string app_id = kPushMessagingAppIdentifierPrefix + origin.spec() +
                       kOriginSWRIdSeparator + guid;

  PushMessagingAppIdentifier app_identifier(app_id, origin,
                                            service_worker_registration_id);
  app_identifier.DCheckValid();
  return app_identifier;
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::FindByAppId(
    Profile* profile, const std::string& app_id) {
  if (!base::StartsWith(app_id, kPushMessagingAppIdentifierPrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return PushMessagingAppIdentifier();
  }

  // Since we now know this is a Push Messaging app_id, check the case hasn't
  // been mangled (crbug.com/461867).
  DCHECK_EQ(kPushMessagingAppIdentifierPrefix, app_id.substr(0, kPrefixLength));
  DCHECK_GE(app_id.size(), kPrefixLength + kGuidLength);
  DCHECK_EQ(app_id.substr(app_id.size() - kGuidLength),
            base::ToUpperASCII(app_id.substr(app_id.size() - kGuidLength)));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);

  std::string map_value;
  if (!map->GetStringWithoutPathExpansion(app_id, &map_value))
    return PushMessagingAppIdentifier();

  GURL origin;
  int64_t service_worker_registration_id;
  if (!GetOriginAndSWRFromPrefValue(map_value, &origin,
                                    &service_worker_registration_id)) {
    NOTREACHED();
    return PushMessagingAppIdentifier();
  }

  PushMessagingAppIdentifier app_identifier(app_id, origin,
                                            service_worker_registration_id);
  app_identifier.DCheckValid();
  return app_identifier;
}

// static
PushMessagingAppIdentifier PushMessagingAppIdentifier::FindByServiceWorker(
    Profile* profile,
    const GURL& origin,
    int64_t service_worker_registration_id) {
  const base::Value pref_value =
      base::Value(MakePrefValue(origin, service_worker_registration_id));

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    if (it.value().Equals(&pref_value))
      return FindByAppId(profile, it.key());
  }
  return PushMessagingAppIdentifier();
}

// static
std::vector<PushMessagingAppIdentifier> PushMessagingAppIdentifier::GetAll(
    Profile* profile) {
  std::vector<PushMessagingAppIdentifier> result;

  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kPushMessagingAppIdentifierMap);
  for (auto it = base::DictionaryValue::Iterator(*map); !it.IsAtEnd();
       it.Advance()) {
    result.push_back(FindByAppId(profile, it.key()));
  }

  return result;
}

// static
void PushMessagingAppIdentifier::DeleteAllFromPrefs(Profile* profile) {
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictionaryValue* map = update.Get();
  map->Clear();
}

// static
size_t PushMessagingAppIdentifier::GetCount(Profile* profile) {
  return profile->GetPrefs()
      ->GetDictionary(prefs::kPushMessagingAppIdentifierMap)
      ->size();
}

PushMessagingAppIdentifier::PushMessagingAppIdentifier()
    : origin_(GURL::EmptyGURL()), service_worker_registration_id_(-1) {}

PushMessagingAppIdentifier::PushMessagingAppIdentifier(
    const std::string& app_id,
    const GURL& origin,
    int64_t service_worker_registration_id)
    : app_id_(app_id),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id) {}

PushMessagingAppIdentifier::~PushMessagingAppIdentifier() {}

void PushMessagingAppIdentifier::PersistToPrefs(Profile* profile) const {
  DCheckValid();

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictionaryValue* map = update.Get();

  // Delete any stale entry with the same origin and Service Worker
  // registration id (hence we ensure there is a 1:1 not 1:many mapping).
  PushMessagingAppIdentifier old =
      FindByServiceWorker(profile, origin_, service_worker_registration_id_);
  if (!old.is_null())
    map->RemoveWithoutPathExpansion(old.app_id_, nullptr /* out_value */);

  map->SetKey(app_id_, base::Value(MakePrefValue(
                           origin_, service_worker_registration_id_)));
}

void PushMessagingAppIdentifier::DeleteFromPrefs(Profile* profile) const {
  DCheckValid();

  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingAppIdentifierMap);
  base::DictionaryValue* map = update.Get();
  map->RemoveWithoutPathExpansion(app_id_, nullptr /* out_value */);
}

void PushMessagingAppIdentifier::DCheckValid() const {
#if DCHECK_IS_ON()
  DCHECK_GE(service_worker_registration_id_, 0);

  DCHECK(origin_.is_valid());
  DCHECK_EQ(origin_.GetOrigin(), origin_);

  // "wp:"
  DCHECK_EQ(kPushMessagingAppIdentifierPrefix,
            app_id_.substr(0, kPrefixLength));

  // Optional (origin.spec() + '#')
  if (app_id_.size() != kPrefixLength + kGuidLength) {
    const size_t suffix_length = 1 /* kOriginSWRIdSeparator */ + kGuidLength;
    DCHECK_GT(app_id_.size(), kPrefixLength + suffix_length);
    DCHECK_EQ(origin_, GURL(app_id_.substr(
                           kPrefixLength,
                           app_id_.size() - kPrefixLength - suffix_length)));
    DCHECK_EQ(std::string(1, kOriginSWRIdSeparator),
              app_id_.substr(app_id_.size() - suffix_length, 1));
  }

  // GUID. In order to distinguish them, an app_id created for an InstanceID
  // based subscription has the last few characters of the GUID overwritten with
  // kInstanceIDGuidSuffix (which contains non-hex characters invalid in GUIDs).
  std::string guid = app_id_.substr(app_id_.size() - kGuidLength);
  if (UseInstanceID(app_id_)) {
    DCHECK(!base::IsValidGUID(guid));

    // Replace suffix with valid hex so we can validate the rest of the string.
    guid = guid.replace(guid.size() - kGuidSuffixLength, kGuidSuffixLength,
                        kGuidSuffixLength, 'C');
  }
  DCHECK(base::IsValidGUID(guid));
#endif  // DCHECK_IS_ON()
}
