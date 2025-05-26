// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_

#include <string_view>

#include "base/feature_list.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/publishers/proto/chrome_app_deprecation.pb.h"

class Profile;
namespace component_updater {
class ComponentUpdateService;
}

// This namespace collects all the methods that control when to enforce the
// multiple stages of the Chrome Apps deprecation for user installed apps, Kiosk
// sessions and managed users.
namespace apps::chrome_app_deprecation {

enum class DeprecationStatus { kLaunchAllowed, kLaunchBlocked };

// Executes all the checks and tasks related to the Chrome Apps deprecation.
DeprecationStatus HandleDeprecation(std::string_view app_id, Profile* profile);

void RegisterAllowlistComponentUpdater(
    component_updater::ComponentUpdateService* cus);

void AddAppToAllowlistForTesting(std::string_view app_id);
void SetKioskSessionForTesting(bool value = true);
void AssignComponentUpdaterAllowlistsForTesting(
    const base::Version& component_version,
    std::optional<const ChromeAppDeprecation::DynamicAllowlists>
        component_data);

BASE_DECLARE_FEATURE(kAllowUserInstalledChromeApps);
BASE_DECLARE_FEATURE(kAllowChromeAppsInKioskSessions);

}  // namespace apps::chrome_app_deprecation

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_
