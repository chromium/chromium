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
//    },
//    <app_id_2> : {
//      "package_name" : <webapk_package_name_2>,
//    },
//    ...
//  },
//  ...
// }
constexpr char kGeneratedWebApksPref[] = "generated_webapks";
constexpr char kPackageNameKey[] = "package_name";

}  // namespace

namespace apps {
namespace webapk_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kGeneratedWebApksPref);
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
  const base::Value* generated_webapks =
      profile->GetPrefs()->GetDictionary(kGeneratedWebApksPref);

  for (auto kv : generated_webapks->DictItems()) {
    ids.insert(kv.first);
  }

  return ids;
}

}  // namespace webapk_prefs
}  // namespace apps
