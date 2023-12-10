// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"

class PrefRegistrySimple;
class Profile;

namespace apps {
namespace webapk_prefs {

extern const char kGeneratedWebApksPref[];
// Name of the pref for whether the Generated WebAPKs feature is enabled,
// controlled by the "ArcAppToWebAppSharingEnabled" policy.
extern const char kGeneratedWebApksEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

void AddWebApk(Profile* profile,
               const std::string& app_id,
               const std::string& package_name);

std::optional<std::string> GetWebApkPackageName(Profile* profile,
                                                const std::string& app_id);

// Returns the package names of all WebAPKs installed in the profile.
base::flat_set<std::string> GetInstalledWebApkPackageNames(Profile* profile);

// Returns the app IDs of all WebAPKs installed in the profile.
base::flat_set<std::string> GetWebApkAppIds(Profile* profile);

// Removes the entry for the WebAPK with the given |package_name|, and returns
// the App Id for the uninstalled package. Returns std::nullopt if no WebAPK
// was installed with the |package_name|.
std::optional<std::string> RemoveWebApkByPackageName(
    Profile* profile,
    const std::string& package_name);

// Marks the given |app_id| as needing (or no longer needing) an update to its
// WebAPK. Does nothing if |app_id| is not associated with an installed WebAPK
// package. This is used to persist the list of apps which need updating, to
// allow updates which fail due to errors or reboots to be retried.
void SetUpdateNeededForApp(Profile* profile,
                           const std::string& app_id,
                           bool update_needed);

// Returns the app IDs of all apps whose installed WebAPK needs to be updated.
base::flat_set<std::string> GetUpdateNeededAppIds(Profile* profile);

}  // namespace webapk_prefs
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_PREFS_H_
