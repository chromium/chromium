// Copyright 2021 The Chromium Authors
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
  ScopedDictPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);

  generated_webapks->EnsureDict(app_id)->Set(kPackageNameKey, package_name);
}

std::optional<std::string> GetWebApkPackageName(Profile* profile,
                                                const std::string& app_id) {
  const base::Value::Dict* app_dict =
      profile->GetPrefs()->GetDict(kGeneratedWebApksPref).FindDict(app_id);
  if (!app_dict) {
    return std::nullopt;
  }

  const std::string* package_name = app_dict->FindString(kPackageNameKey);
  if (!package_name) {
    return std::nullopt;
  }

  return *package_name;
}

base::flat_set<std::string> GetWebApkAppIds(Profile* profile) {
  base::flat_set<std::string> ids;
  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetDict(kGeneratedWebApksPref);

  for (const auto kv : generated_webapks) {
    ids.insert(kv.first);
  }

  return ids;
}

base::flat_set<std::string> GetInstalledWebApkPackageNames(Profile* profile) {
  base::flat_set<std::string> package_names;

  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetDict(kGeneratedWebApksPref);

  for (const auto kv : generated_webapks) {
    const std::string* package_name =
        kv.second.GetDict().FindString(kPackageNameKey);
    DCHECK(package_name);
    package_names.insert(*package_name);
  }

  return package_names;
}

std::optional<std::string> RemoveWebApkByPackageName(
    Profile* profile,
    const std::string& package_name) {
  ScopedDictPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);

  for (auto kv : *generated_webapks) {
    const std::string* item_package_name =
        kv.second.GetDict().FindString(kPackageNameKey);
    if (item_package_name && *item_package_name == package_name) {
      std::string app_id = kv.first;
      generated_webapks->Remove(kv.first);
      return app_id;
    }
  }

  return std::nullopt;
}

void SetUpdateNeededForApp(Profile* profile,
                           const std::string& app_id,
                           bool update_needed) {
  ScopedDictPrefUpdate generated_webapks(profile->GetPrefs(),
                                         kGeneratedWebApksPref);
  base::Value::Dict* app_dict = generated_webapks->FindDict(app_id);
  if (app_dict) {
    app_dict->Set(kUpdateNeededKey, update_needed);
  }
}

base::flat_set<std::string> GetUpdateNeededAppIds(Profile* profile) {
  base::flat_set<std::string> ids;
  const base::Value::Dict& generated_webapks =
      profile->GetPrefs()->GetDict(kGeneratedWebApksPref);

  for (auto kv : generated_webapks) {
    std::optional<bool> update_needed =
        kv.second.GetDict().FindBool(kUpdateNeededKey);
    if (update_needed.has_value() && update_needed.value()) {
      ids.insert(kv.first);
    }
  }

  return ids;
}

}  // namespace webapk_prefs
}  // namespace apps
