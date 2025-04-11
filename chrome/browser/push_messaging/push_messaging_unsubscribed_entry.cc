// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_unsubscribed_entry.h"

#include <string>
#include <string_view>

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

// Ok to use '#' as separator since only the origin of the url is used.
constexpr char kPrefValueSeparator = '#';

std::string MakePrefValue(const GURL& origin,
                          int64_t service_worker_registration_id) {
  CHECK(!origin.has_ref());
  return origin.spec() + kPrefValueSeparator +
         base::NumberToString(service_worker_registration_id);
}

std::optional<PushMessagingUnsubscribedEntry> DisassemblePrefValue(
    std::string_view pref_value) {
  std::vector<std::string_view> parts =
      base::SplitStringPiece(pref_value, std::string(1, kPrefValueSeparator),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (parts.size() < 2) {
    return std::nullopt;
  }

  int64_t service_worker_registration_id;
  if (!base::StringToInt64(parts[1], &service_worker_registration_id)) {
    return std::nullopt;
  }

  GURL origin = GURL(parts[0]);
  if (!origin.is_valid()) {
    return std::nullopt;
  }

  return PushMessagingUnsubscribedEntry(std::move(origin),
                                        service_worker_registration_id);
}

}  // namespace

// static
void PushMessagingUnsubscribedEntry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPushMessagingUnsubscribedEntriesList);
}

PushMessagingUnsubscribedEntry::PushMessagingUnsubscribedEntry(
    GURL origin,
    int64_t service_worker_registration_id)
    : origin_(std::move(origin)),
      service_worker_registration_id_(service_worker_registration_id) {}

// static
std::vector<PushMessagingUnsubscribedEntry>
PushMessagingUnsubscribedEntry::GetAll(Profile* profile) {
  std::vector<PushMessagingUnsubscribedEntry> result;

  const base::Value::List& list = profile->GetPrefs()->GetList(
      prefs::kPushMessagingUnsubscribedEntriesList);
  for (const auto& entry : list) {
    if (entry.is_string()) {
      std::optional<PushMessagingUnsubscribedEntry> parsed_entry =
          DisassemblePrefValue(entry.GetString());
      if (parsed_entry) {
        result.push_back(std::move(parsed_entry).value());
      }
    }
  }

  return result;
}

// static
void PushMessagingUnsubscribedEntry::DeleteAllFromPrefs(Profile* profile) {
  profile->GetPrefs()->SetList(prefs::kPushMessagingUnsubscribedEntriesList,
                               base::Value::List());
}

void PushMessagingUnsubscribedEntry::PersistToPrefs(Profile* profile) const {
  DCheckValid();

  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingUnsubscribedEntriesList);
  base::Value::List& list = update.Get();

  std::string pref_value =
      MakePrefValue(origin_, service_worker_registration_id_);
  // Delete any stale entry with the same origin and Service Worker.
  list.EraseIf([&pref_value](const base::Value& entry) {
    return !entry.is_string() || entry.GetString() == pref_value;
  });
  list.Append(pref_value);
}

void PushMessagingUnsubscribedEntry::DeleteFromPrefs(Profile* profile) const {
  DCheckValid();
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kPushMessagingUnsubscribedEntriesList);
  base::Value::List& list = update.Get();
  std::string pref_value =
      MakePrefValue(origin_, service_worker_registration_id_);
  list.EraseIf([&pref_value](const base::Value& entry) {
    return !entry.is_string() || entry.GetString() == pref_value;
  });
}

void PushMessagingUnsubscribedEntry::DCheckValid() const {
#if DCHECK_IS_ON()
  DCHECK_GE(service_worker_registration_id_, 0);

  DCHECK(origin_.is_valid());
  DCHECK_EQ(origin_.DeprecatedGetOriginAsURL(), origin_);
#endif  // DCHECK_IS_ON()
}
