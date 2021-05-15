// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class Profile;

namespace apps {
namespace webapk_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry);

void AddWebApk(Profile* profile,
               const std::string& app_id,
               const std::string& package_name);

absl::optional<std::string> GetWebApkPackageName(Profile* profile,
                                                 const std::string& app_id);

// Returns the app IDs of all WebAPKs installed in the profile.
base::flat_set<std::string> GetWebApkAppIds(Profile* profile);

}  // namespace webapk_prefs
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_
