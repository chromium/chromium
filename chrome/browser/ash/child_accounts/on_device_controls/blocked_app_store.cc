// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_store.h"

#include "ash/constants/ash_pref_names.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::on_device_controls {

namespace {

constexpr char kAppIdKey[] = "app_id";
constexpr char kBlockTimestampKey[] = "block_timestamp";
constexpr char kUninstallTimestampKey[] = "uninstall_timestamp";

}  // namespace

// static
void BlockedAppStore::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kOnDeviceAppControlsBlockedApps);
}

BlockedAppStore::BlockedAppStore(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

BlockedAppStore::~BlockedAppStore() = default;

BlockedAppMap BlockedAppStore::GetFromPref() const {
  VLOG(1) << "app-controls: reading blocked apps from pref ";

  const base::Value::List& list =
      pref_service_->GetList(prefs::kOnDeviceAppControlsBlockedApps);

  BlockedAppMap apps_from_pref;
  for (const auto& item : list) {
    const base::Value::Dict* dict = item.GetIfDict();
    if (!dict) {
      LOG(WARNING) << "app-controls: invalid block app dictionary";
      continue;
    }

    const std::string* app_id = dict->FindString(kAppIdKey);
    if (!app_id) {
      LOG(WARNING) << "app-controls: invalid app id string";
      continue;
    }

    std::optional<base::Time> block_timestamp =
        base::ValueToTime(dict->Find(kBlockTimestampKey));
    if (!block_timestamp) {
      LOG(WARNING) << "app-controls: invalid app blocked timestamp";
      continue;
    }

    std::optional<base::Time> uninstall_timestamp =
        base::ValueToTime(dict->Find(kUninstallTimestampKey));

    apps_from_pref[*app_id] =
        uninstall_timestamp
            ? BlockedAppDetails(*block_timestamp, *uninstall_timestamp)
            : BlockedAppDetails(*block_timestamp);
  }
  return apps_from_pref;
}

void BlockedAppStore::SaveToPref(const BlockedAppMap& apps) {
  VLOG(1) << "app-controls: saving blocked apps to pref ";

  ScopedListPrefUpdate update(pref_service_,
                              prefs::kOnDeviceAppControlsBlockedApps);

  base::Value::List& list = update.Get();
  list.clear();

  for (const auto& app : apps) {
    const std::string& app_id = app.first;
    const BlockedAppDetails& app_details = app.second;

    base::Value::Dict dict;
    dict.Set(kAppIdKey, app_id);
    dict.Set(kBlockTimestampKey,
             base::TimeToValue(app_details.block_timestamp()));
    if (!app_details.IsInstalled()) {
      dict.Set(kUninstallTimestampKey,
               base::TimeToValue(app_details.uninstall_timestamp().value()));
    }
    list.Append(std::move(dict));
  }
  pref_service_->CommitPendingWrite();
}

}  // namespace ash::on_device_controls
