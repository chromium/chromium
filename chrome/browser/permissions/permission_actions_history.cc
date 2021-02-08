// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_actions_history.h"

#include "base/containers/adapters.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

#include <vector>

namespace {
// Inner structure of |prefs::kPermissionActions| containing a history of past
// permission actions. It is a dictionary of JSON lists keyed on the result of
// PermissionUtil::GetPermissionString (lower-cased for backwards compatibility)
// and has the following format:
//
//   "profile.content_settings.permission_actions": {
//      "notifications": [
//       { "time": "1333333333337", "action": 1 },
//       { "time": "1567957177000", "action": 3 },
//     ],
//     "geolocation": [...],
//     ...
//   }
constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";

// Entries in permission actions expire after they become this old.
constexpr base::TimeDelta kPermissionActionMaxAge =
    base::TimeDelta::FromDays(90);

}  // namespace

// static
PermissionActionsHistory* PermissionActionsHistory::GetForProfile(
    Profile* profile) {
  return Factory::GetForProfile(profile);
}

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistory(const base::Time& begin) {
  const base::DictionaryValue* dictionary =
      pref_service_->GetDictionary(prefs::kPermissionActions);
  if (!dictionary)
    return {};

  std::vector<PermissionActionsHistory::Entry> matching_actions;
  for (const auto& permission_entry : *dictionary) {
    const auto permission_actions =
        GetHistoryInternal(begin, permission_entry.first);

    matching_actions.insert(matching_actions.end(), permission_actions.begin(),
                            permission_actions.end());
  }

  base::ranges::sort(matching_actions, {},
                     [](const PermissionActionsHistory::Entry& entry) {
                       return entry.time;
                     });

  return matching_actions;
}

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistory(const base::Time& begin,
                                          permissions::RequestType type) {
  return GetHistoryInternal(begin, PermissionKeyForRequestType(type));
}

void PermissionActionsHistory::RecordAction(
    permissions::PermissionAction action,
    permissions::RequestType type) {
  DictionaryPrefUpdate update(pref_service_, prefs::kPermissionActions);

  const base::StringPiece permission_path(PermissionKeyForRequestType(type));

  if (!update->FindPathOfType(permission_path, base::Value::Type::LIST)) {
    update->SetPath(permission_path, base::ListValue());
  }

  base::Value* permission_actions =
      update->FindPathOfType(permission_path, base::Value::Type::LIST);
  CHECK(permission_actions);

  // Discard permission actions older than |kPermissionActionMaxAge|.
  const base::Time cutoff = base::Time::Now() - kPermissionActionMaxAge;
  permission_actions->EraseListValueIf([cutoff](const base::Value& entry) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
    return !timestamp || *timestamp < cutoff;
  });

  // Record the new permission action.
  base::DictionaryValue new_action_attributes;
  new_action_attributes.SetKey(kPermissionActionEntryTimestampKey,
                               util::TimeToValue(base::Time::Now()));
  new_action_attributes.SetIntKey(kPermissionActionEntryActionKey,
                                  static_cast<int>(action));
  permission_actions->Append(std::move(new_action_attributes));
}

void PermissionActionsHistory::ClearHistory(const base::Time& delete_begin,
                                                 const base::Time& delete_end) {
  DCHECK(!delete_end.is_null());
  if (delete_begin.is_null() && delete_end.is_max()) {
    pref_service_->ClearPref(prefs::kPermissionActions);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, prefs::kPermissionActions);

  for (const auto& permission_entry : *update) {
    permission_entry.second->EraseListValueIf([delete_begin,
                                               delete_end](const auto& entry) {
      const base::Optional<base::Time> timestamp =
          util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
      return (!timestamp ||
              (*timestamp >= delete_begin && *timestamp < delete_end));
    });
  }
}

PermissionActionsHistory::PermissionActionsHistory(Profile* profile)
    : pref_service_(profile->GetPrefs()) {}

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistoryInternal(const base::Time& begin,
                                                  const std::string& key) {
  const base::Value* permission_actions =
      pref_service_->GetDictionary(prefs::kPermissionActions)->FindListKey(key);

  if (!permission_actions)
    return {};

  std::vector<Entry> matching_actions;

  for (const auto& entry : permission_actions->GetList()) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));

    if (timestamp >= begin) {
      const permissions::PermissionAction past_action =
          static_cast<permissions::PermissionAction>(
              *(entry.FindIntKey(kPermissionActionEntryActionKey)));

      matching_actions.emplace_back(
          PermissionActionsHistory::Entry{past_action, timestamp.value()});
    }
  }

  return matching_actions;
}

// PermissionActionsHistory::Factory------------------------------------
PermissionActionsHistory*
PermissionActionsHistory::Factory::GetForProfile(Profile* profile) {
  return static_cast<PermissionActionsHistory*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionActionsHistory::Factory*
PermissionActionsHistory::Factory::GetInstance() {
  return base::Singleton<PermissionActionsHistory::Factory>::get();
}

PermissionActionsHistory::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "PermissionActionsHistory",
          BrowserContextDependencyManager::GetInstance()) {}

PermissionActionsHistory::Factory::~Factory() = default;

KeyedService* PermissionActionsHistory::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PermissionActionsHistory(static_cast<Profile*>(context));
}

content::BrowserContext*
PermissionActionsHistory::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
