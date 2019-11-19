// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
namespace api {
namespace app_runtime {
struct ActionData;
}  // namespace app_runtime
}  // namespace api
}  // namespace extensions

namespace chromeos {

class NoteTakingControllerClient;

// Describes an app's level of support for lock screen enabled note taking.
// IMPORTANT: These constants are used in settings UI, so be careful about
//     reordering/adding/removing items.
enum class NoteTakingLockScreenSupport {
  // The app does not support note taking on lock screen.
  kNotSupported = 0,
  // The app supports lock screen note taking, but is not allowed to run on the
  // lock screen due to policy settings.
  kNotAllowedByPolicy = 1,
  // The app supports note taking on lock screen, but is not enabled as a
  // lock screen note taking app by the user. This state implies that the user
  // can be offered to enable this app as the lock screen note taking handler.
  kSupported = 2,
  // The app is enabled by the user to run as a note taking handler on the lock
  // screen. Note that, while more than one app can be in enabled state at a
  // same time, currently only the preferred note taking app will be launchable
  // from the lock screen UI.
  kEnabled = 3,
};

// Information about an installed note-taking app.
struct NoteTakingAppInfo {
  // Application name to display to user.
  std::string name;

  // Either an extension ID (in the case of a Chrome app) or a package name (in
  // the case of an Android app).
  std::string app_id;

  // True if this is the preferred note-taking app.
  bool preferred;

  // Whether the app supports taking notes on Chrome OS lock screen. Note that
  // this ability is guarded by enable-lock-screen-apps feature flag, and
  // whitelisted to Keep apps.
  NoteTakingLockScreenSupport lock_screen_support;
};

using NoteTakingAppInfos = std::vector<NoteTakingAppInfo>;

// Singleton class used to launch a note-taking app.
class NoteTakingHelper : public arc::ArcIntentHelperObserver,
                         public arc::ArcSessionManager::Observer,
                         public extensions::ExtensionRegistryObserver,
                         public ProfileManagerObserver {
 public:
  // Interface for observing changes to the list of available apps.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the list of available apps that will be returned by
    // GetAvailableApps() changes or when |play_store_enabled_| changes state.
    virtual void OnAvailableNoteTakingAppsUpdated() = 0;

    // Called when the preferred note taking app (or its properties) in
    // |profile| is updated.
    virtual void OnPreferredNoteTakingAppUpdated(Profile* profile) = 0;
  };

  // Describes the result of an attempt to launch a note-taking app. Values must
  // not be renumbered, as this is used by histogram metrics.
  enum class LaunchResult {
    // A Chrome app was launched successfully.
    CHROME_SUCCESS = 0,
    // The requested Chrome app was unavailable.
    CHROME_APP_MISSING = 1,
    // An Android app was launched successfully.
    ANDROID_SUCCESS = 2,
    // An Android app couldn't be launched due to the profile not being allowed
    // to use ARC.
    ANDROID_NOT_SUPPORTED_BY_PROFILE = 3,
    // An Android app couldn't be launched due to ARC not running.
    ANDROID_NOT_RUNNING = 4,
    // An Android app couldn't be launched due to a failure to convert the
    // supplied path to an ARC URL.
    ANDROID_FAILED_TO_CONVERT_PATH = 5,
    // No attempt was made due to a preferred app not being specified.
    NO_APP_SPECIFIED = 6,
    // No Android or Chrome apps were available.
    NO_APPS_AVAILABLE = 7,
    // This value must remain last and should be incremented when a new reason
    // is inserted.
    MAX = 8,
  };

  // Callback used to launch a Chrome app.
  using LaunchChromeAppCallback = base::Callback<void(
      content::BrowserContext* context,
      const extensions::Extension*,
      std::unique_ptr<extensions::api::app_runtime::ActionData>,
      const base::FilePath&)>;

  // Intent action used to launch Android apps.
  static const char kIntentAction[];

  // Extension IDs for the development and released versions of the Google Keep
  // Chrome app.
  static const char kDevKeepExtensionId[];
  static const char kProdKeepExtensionId[];

  // Names of histograms.
  static const char kPreferredLaunchResultHistogramName[];
  static const char kDefaultLaunchResultHistogramName[];

  static void Initialize();
  static void Shutdown();
  static NoteTakingHelper* Get();

  bool play_store_enabled() const { return play_store_enabled_; }
  bool android_apps_received() const { return android_apps_received_; }

  void set_launch_chrome_app_callback_for_test(
      const LaunchChromeAppCallback& callback) {
    launch_chrome_app_callback_ = callback;
  }

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a list of available note-taking apps.
  std::vector<NoteTakingAppInfo> GetAvailableApps(Profile* profile);

  // Returns the preferred app info, if the preferred app exists and is a Chrome
  // app.
  std::unique_ptr<NoteTakingAppInfo> GetPreferredChromeAppInfo(
      Profile* profile);

  // Sets the preferred note-taking app. |app_id| is a value from a
  // NoteTakingAppInfo object.
  void SetPreferredApp(Profile* profile, const std::string& app_id);

  // Enables or disables preferred note taking apps from running on the lock
  // screen.
  // Returns whether the app status changed.
  bool SetPreferredAppEnabledOnLockScreen(Profile* profile, bool enabled);

  // Returns true if an app that can be used to take notes is available. UI
  // surfaces that call LaunchAppForNewNote() should be hidden otherwise.
  bool IsAppAvailable(Profile* profile);

  // Launches the note-taking app to create a new note, optionally additionally
  // passing a file (|path| may be empty). IsAppAvailable() must be called
  // first.
  void LaunchAppForNewNote(Profile* profile, const base::FilePath& path);

  // arc::ArcIntentHelperObserver:
  void OnIntentFiltersUpdated(
      const base::Optional<std::string>& package_name) override;

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // Sets the profile which supports note taking apps on the lock screen.
  void SetProfileWithEnabledLockScreenApps(Profile* profile);

  NoteTakingControllerClient* GetNoteTakingControllerClientForTesting() {
    return note_taking_controller_client_.get();
  }

 private:
  // The state of app ID whitelist cache (used for determining the state of
  // note-taking apps whtielisted for the lock screen).
  enum class AppWhitelistState {
    // The whitelist value has not yet been determined.
    kUndetermined,
    // The app ID whitelist does not exist in the profile.
    kNoAppWhitelist,
    // The app ID whitelist exists in the profile.
    kAppsWhitelisted
  };

  NoteTakingHelper();
  ~NoteTakingHelper() override;

  // Returns true if |extension| is a whitelisted note-taking app and false
  // otherwise.
  bool IsWhitelistedChromeApp(const extensions::Extension* extension) const;

  // Queries and returns all installed and enabled whitelisted Chrome
  // note-taking apps for |profile|.
  std::vector<const extensions::Extension*> GetChromeApps(
      Profile* profile) const;

  // Requests a list of Android note-taking apps from ARC.
  void UpdateAndroidApps();

  // Handles ARC's response to an earlier UpdateAndroidApps() call.
  void OnGotAndroidApps(std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

  // Helper method that launches |app_id| (either an Android package name or a
  // Chrome extension ID) to create a new note with an optional attached file at
  // |path|. Returns the attempt's result.
  LaunchResult LaunchAppInternal(Profile* profile,
                                 const std::string& app_id,
                                 const base::FilePath& path);

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // Determines the state of the |app|'s support for lock screen note taking.
  // |profile| - The profile in which the app is installed.
  NoteTakingLockScreenSupport GetLockScreenSupportForChromeApp(
      Profile* profile,
      const extensions::Extension* app);

  // Called when kNoteTakingAppsLockScreenWhitelist pref changes for
  // |profile_with_enabled_lock_screen_apps_|.
  void OnAllowedNoteTakingAppsChanged();

  // Updates the cached whitelist of note-taking apps allowed on the lock
  // screen - it sets |lock_screen_whitelist_state_|  and
  // |lock_screen_apps_allowed_by_policy_| to values appropriate for the current
  // |profile_with_enabled_lock_screen_apps_| state.
  void UpdateLockScreenAppsWhitelistState();

  // True iff Play Store is enabled (i.e. per the checkbox on the settings
  // page). Note that ARC may not be fully started yet when this is true, but it
  // is expected to start eventually. Similarly, ARC may not be fully shut down
  // yet when this is false, but will be eventually.
  bool play_store_enabled_ = false;

  // This is set to true after |android_apps_| is updated.
  bool android_apps_received_ = false;

  // Callback used to launch Chrome apps. Can be overridden for tests.
  LaunchChromeAppCallback launch_chrome_app_callback_;

  // Extension IDs of whitelisted (but not necessarily installed) Chrome
  // note-taking apps in the order in which they're chosen if the user hasn't
  // expressed a preference.
  std::vector<extensions::ExtensionId> whitelisted_chrome_app_ids_;

  // Cached information about available Android note-taking apps.
  NoteTakingAppInfos android_apps_;

  // Tracks ExtensionRegistry observation for different profiles.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // The profile for which lock screen apps are enabled,
  Profile* profile_with_enabled_lock_screen_apps_ = nullptr;

  // The current AppWhitelistState for lock screen note taking in
  // |profile_with_enabled_lock_screen_apps_|. If kAppsWhitelisted,
  // |lock_screen_apps_allowed_by_policy_| should contain the set of whitelisted
  // app IDs.
  AppWhitelistState lock_screen_whitelist_state_ =
      AppWhitelistState::kUndetermined;

  // If |lock_screen_whitelist_state_| is kAppsWhitelisted, contains all app
  // IDs that are allowed to handle new-note action on the lock screen. The set
  // should only be used for apps from |profile_with_enabled_lock_screen_apps_|
  // and when |lock_screen_whitelist_state_| equals kAppsWhitelisted.
  std::set<std::string> lock_screen_apps_allowed_by_policy_;

  // Tracks kNoteTakingAppsLockScreenWhitelist pref for the profile for which
  // lock screen apps are enabled.
  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<NoteTakingControllerClient> note_taking_controller_client_;

  base::WeakPtrFactory<NoteTakingHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
