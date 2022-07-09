// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// The pref dict is:
// {
//  ...
//  "generated_webapks" : {
//    <app_id_1> : {
//      "package_name" : <webapk_package_name_1>
//      "update_needed" : <bool>
//    },
//    <app_id_2> : {
//      "package_name" : <webapk_package_name_2>,
//    },
//    ...
//  },
//  ...
// }

constexpr char kPackageNameKey[] = "package_name";
constexpr char kUpdateNeededKey[] = "update_needed";

}  // namespace

namespace apps {
namespace webapk_prefs {

const char kGeneratedWebApksPref[] = "generated_webapks";
const char kGeneratedWebApksEnabled[] = "generated_webapks_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kGeneratedWebApksPref);
  registry->RegisterBooleanPref(kGeneratedWebApksEnabled, true);
}

void AddWebApk(Profile* profile,
               const std::string& app_id,
               const std::string& package_name) {
  DictionaryPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);

  generated_webapks->SetPath({app_id, kPackageNameKey},
                             base::Value(package_name));
}

absl::optional<std::string> GetWebApkPackageName(Profile* profile,
                                                 const std::string& app_id) {
  const base::Value* app_dict = profile->GetPrefs()
                                    ->GetDictionary(kGeneratedWebApksPref)
                                    ->FindDictKey(app_id);
  if (!app_dict) {
    return absl::nullopt;
  }

  const std::string* package_name = app_dict->FindStringKey(kPackageNameKey);
  if (!package_name) {
    return absl::nullopt;
  }

  return *package_name;
}

base::flat_set<std::string> GetWebApkAppIds(Profile* profile) {
  base::flat_set<std::string> ids;
  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetValueDict(kGeneratedWebApksPref);

  for (const auto kv : generated_webapks) {
    ids.insert(kv.first);
  }

  return ids;
}

base::flat_set<std::string> GetInstalledWebApkPackageNames(Profile* profile) {
  base::flat_set<std::string> package_names;

  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetValueDict(kGeneratedWebApksPref);

  for (const auto kv : generated_webapks) {
    const std::string* package_name = kv.second.FindStringKey(kPackageNameKey);
    DCHECK(package_name);
    package_names.insert(*package_name);
  }

  return package_names;
}

absl::optional<std::string> RemoveWebApkByPackageName(
    Profile* profile,
    const std::string& package_name) {
  DictionaryPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);

  for (auto kv : generated_webapks->DictItems()) {
    const std::string* item_package_name =
        kv.second.FindStringKey(kPackageNameKey);
    if (item_package_name && *item_package_name == package_name) {
      std::string app_id = kv.first;
      generated_webapks->RemoveKey(kv.first);
      return app_id;
    }
  }

  return absl::nullopt;
}

void SetUpdateNeededForApp(Profile* profile,
                           const std::string& app_id,
                           bool update_needed) {
  DictionaryPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);
  if (generated_webapks->FindKey(app_id)) {
    generated_webapks->SetPath({app_id, kUpdateNeededKey},
                               base::Value(update_needed));
  }
}

base::flat_set<std::string> GetUpdateNeededAppIds(Profile* profile) {
  base::flat_set<std::string> ids;
  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetValueDict(kGeneratedWebApksPref);

  for (auto kv : generated_webapks) {
    absl::optional<bool> update_needed =
        kv.second.FindBoolKey(kUpdateNeededKey);
    if (update_needed.has_value() && update_needed.value()) {
      ids.insert(kv.first);
    }
  }

  return ids;
}

}  // namespace webapk_prefs
}  // namespace apps
