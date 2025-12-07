// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_CHROME_APP_DEPRECATION_CHROME_APP_DEPRECATION_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_CHROME_APP_DEPRECATION_CHROME_APP_DEPRECATION_H_

#include <string_view>

#include "base/feature_list.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/chrome_app_deprecation/proto/chrome_app_deprecation.pb.h"

class Profile;

namespace base {
class FilePath;
}

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

BASE_DECLARE_FEATURE(kAllowUserInstalledChromeApps);
BASE_DECLARE_FEATURE(kAllowChromeAppsInKioskSessions);

// ---------- Testing entry points -----------------
void SetKioskSessionForTesting(bool value);
void AssignComponentUpdaterAllowlistsForTesting(
    const base::Version& component_version,
    std::optional<const ChromeAppDeprecation::DynamicAllowlists>
        component_data);
void LoadComponentUpdaterAllowlistsForTesting(
    const base::Version& component_version,
    const base::FilePath& file_path);
extern bool g_load_component_updater_allowlists_complete_for_testing;

class ScopedSkipSystemDialogForTesting {
 public:
  ScopedSkipSystemDialogForTesting();
  ScopedSkipSystemDialogForTesting(const ScopedSkipSystemDialogForTesting&) =
      delete;
  ScopedSkipSystemDialogForTesting& operator=(
      const ScopedSkipSystemDialogForTesting&) = delete;
  ~ScopedSkipSystemDialogForTesting();
};

class ScopedAddAppToAllowlistForTesting {
 public:
  explicit ScopedAddAppToAllowlistForTesting(std::string app_id_);
  ScopedAddAppToAllowlistForTesting(const ScopedAddAppToAllowlistForTesting&) =
      delete;
  ScopedAddAppToAllowlistForTesting& operator=(
      const ScopedAddAppToAllowlistForTesting&) = delete;
  ~ScopedAddAppToAllowlistForTesting();

 private:
  std::string app_id_;
};
}  // namespace apps::chrome_app_deprecation

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_CHROME_APP_DEPRECATION_CHROME_APP_DEPRECATION_H_
