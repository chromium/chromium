// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_screen_extensions_storage_cleaner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"

namespace ash {
namespace {

const char kPersistentDataKeyPrefix[] = "persistent_data_";

}  // namespace

LoginScreenExtensionsStorageCleaner::LoginScreenExtensionsStorageCleaner() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  prefs_ = ProfileHelper::GetSigninProfile()->GetPrefs();
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      extensions::pref_names::kInstallForceList,
      base::BindRepeating(&LoginScreenExtensionsStorageCleaner::OnPolicyUpdated,
                          base::Unretained(this)));
}

LoginScreenExtensionsStorageCleaner::~LoginScreenExtensionsStorageCleaner() =
    default;

void LoginScreenExtensionsStorageCleaner::OnPolicyUpdated() {
  ClearPersistentDataForUninstalledExtensions();
}

void LoginScreenExtensionsStorageCleaner::
    ClearPersistentDataForUninstalledExtensions() {
  std::vector<std::string> installed_extension_ids;
  const PrefService::Preference* const pref =
      prefs_->FindPreference(extensions::pref_names::kInstallForceList);
  if (pref && pref->IsManaged() && pref->GetType() == base::Value::Type::DICT) {
    // Each `item` contains a pair of extension ID and update URL.
    for (const auto item : pref->GetValue()->DictItems())
      installed_extension_ids.push_back(item.first);
  }
  SessionManagerClient::Get()->LoginScreenStorageListKeys(base::BindOnce(
      &LoginScreenExtensionsStorageCleaner::
          ClearPersistentDataForUninstalledExtensionsImpl,
      base::Unretained(this), std::move(installed_extension_ids)));
}

void LoginScreenExtensionsStorageCleaner::
    ClearPersistentDataForUninstalledExtensionsImpl(
        const std::vector<std::string>& installed_extension_ids,
        std::vector<std::string> keys,
        absl::optional<std::string> error) {
  if (error)
    return;

  for (const std::string& key : keys) {
    // Skip the keys that are not extension's persistent data.
    if (!base::StartsWith(key, kPersistentDataKeyPrefix,
                          base::CompareCase::SENSITIVE))
      continue;

    // If no installed extension has created the key, delete it.
    bool has_owner_extension = false;
    for (const std::string& extension_id : installed_extension_ids) {
      if (key.find(extension_id) != std::string::npos) {
        has_owner_extension = true;
        break;
      }
    }
    if (!has_owner_extension)
      SessionManagerClient::Get()->LoginScreenStorageDelete(key);
  }
}

}  // namespace ash
