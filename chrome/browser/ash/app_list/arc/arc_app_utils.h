// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_UTILS_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/services/app_service/public/cpp/intent.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace arc {

// Observes ARC app launches.
class AppLaunchObserver : public base::CheckedObserver {
 public:
  // Called when an app launch is requested
  virtual void OnAppLaunchRequested(const ArcAppListPrefs::AppInfo& app_info) {}
};

// Checks if a given app should be hidden in launcher.
bool ShouldShowInLauncher(const std::string& app_id);

// Helper to create arc::mojom::WindowInfoPtr using |display_id|, which is the
// id of the display from which the app is launched.
arc::mojom::WindowInfoPtr MakeWindowInfo(int64_t display_id);

// Launches an ARC app.
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               UserInteractionType user_action);
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               UserInteractionType user_action,
               arc::mojom::WindowInfoPtr window_info);

bool LaunchAppWithIntent(content::BrowserContext* context,
                         const std::string& app_id,
                         apps::IntentPtr launch_intent,
                         int event_flags,
                         UserInteractionType user_action,
                         arc::mojom::WindowInfoPtr window_info);

// Launches App Shortcut that was published by Android's ShortcutManager.
bool LaunchAppShortcutItem(content::BrowserContext* context,
                           const std::string& app_id,
                           const std::string& shortcut_id,
                           int64_t display_id);

// Updates pre-launched window info to ARC.
void UpdateWindowInfo(arc::mojom::WindowInfoPtr window_info);

// Sets task active.
void SetTaskActive(int task_id);

// Closes the task.
void CloseTask(int task_id);

// Sets TouchMode in Android. Returns true if the intent was sent.
bool SetTouchMode(bool enable);

// Gets user selected package names.
std::vector<std::string> GetSelectedPackagesFromPrefs(
    content::BrowserContext* context);

// Starts Play Fast App Reinstall flow.
void StartFastAppReinstallFlow(const std::vector<std::string>& package_names);

// Uninstalls the package in ARC.
void UninstallPackage(const std::string& package_name);

// Uninstalls ARC app or removes shortcut.
void UninstallArcApp(const std::string& app_id, Profile* profile);

// Removes cached app shortcut icon in ARC.
void RemoveCachedIcon(const std::string& icon_resource_id);

// Shows package info for ARC package at the specified page.
bool ShowPackageInfo(const std::string& package_name,
                     mojom::ShowPackageInfoPage page,
                     int64_t display_id);

// Returns true if |id| represents either ARC app or ARC shelf group.
bool IsArcItem(content::BrowserContext* context, const std::string& id);

// Returns current active locale and list of preferred languages for the given
// |profile|.
void GetLocaleAndPreferredLanguages(const Profile* profle,
                                    std::string* out_locale,
                                    std::string* out_preferred_languages);

// Returns Android instance id. Result is returned in callback. |ok| is set to
// true in case app instance is ready and Android id was successfully requested.
// 0 is reserved for |android_id| for unregistered Android instances, however
// this should not happen normally because app instance is active after ARC
// provisioning is done.
void GetAndroidId(
    base::OnceCallback<void(bool ok, int64_t android_id)> callback);

// Returns the Arc package name for the specified app_id, which must
// be the AppID of an ARC app.
std::string AppIdToArcPackageName(const std::string& app_id, Profile* profile);

// Returns the AppID for the specified package_name, which must be the package
// name of an ARC app or an empty string if name not found.
std::string ArcPackageNameToAppId(const std::string& package_name,
                                  Profile* profile);

// Add/remove an observer to be notified of app launches.
void AddAppLaunchObserver(content::BrowserContext* context,
                          AppLaunchObserver* observer);
void RemoveAppLaunchObserver(content::BrowserContext* context,
                             AppLaunchObserver* observer);

// Returns the app id from the app id or the shelf group id.
const std::string GetAppFromAppOrGroupId(content::BrowserContext* context,
                                         const std::string& app_or_group_id);

// Executes an app Shortcut command.
void ExecuteArcShortcutCommand(content::BrowserContext* context,
                               const std::string& id,
                               const std::string& shortcut_id,
                               int64_t display_id);

// Records whether or not Play Store has been launched by the user within a
// week after from the when onboarding (OOBE) finished, following this logic:
// * we are still within a week from onboarding:
//   -Play Store has been launched --> true
//   -Play Store has not been launched yet --> do nothing
// * a week has passed since onboarding --> no
void RecordPlayStoreLaunchWithinAWeek(PrefService* prefs, bool launched);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_UTILS_H_
