// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_HELPER_H_
#define CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace apps {
class AppUpdate;
}

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

namespace ash {

class NoteTakingControllerClient;

// Information about an installed note-taking app.
struct NoteTakingAppInfo {
  // Application name to display to user.
  std::string name;

  // Either an extension ID (in the case of a Chrome app) or a package name (in
  // the case of an Android app) or a web app ID (in the case of a web app).
  std::string app_id;

  // True if this is the preferred note-taking app.
  bool preferred;

  // Whether the app supports use on the Chrome OS lock screen.
  LockScreenAppSupport lock_screen_support;
};

// Singleton class used to launch a note-taking app.
class NoteTakingHelper : public arc::ArcIntentHelperObserver,
                         public arc::ArcSessionManagerObserver,
                         public apps::AppRegistryCache::Observer,
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
    // Removed: An Android app couldn't be launched due to a failure to convert
    // the supplied path to an ARC URL.
    // ANDROID_FAILED_TO_CONVERT_PATH = 5,
    // No attempt was made due to a preferred app not being specified.
    NO_APP_SPECIFIED = 6,
    // No Android or Chrome apps were available.
    NO_APPS_AVAILABLE = 7,
    // A web app was launched successfully.
    WEB_APP_SUCCESS = 8,
    // The requested web app was unavailable.
    WEB_APP_MISSING = 9,
    // Unable to find an internal display.
    NO_INTERNAL_DISPLAY_FOUND = 10,
    // This value must remain last and should be incremented when a new reason
    // is inserted.
    MAX = 11,
  };

  // Callback used to launch a Chrome app.
  using LaunchChromeAppCallback =
      base::RepeatingCallback<void(content::BrowserContext* context,
                                   const extensions::Extension*,
                                   extensions::api::app_runtime::ActionData)>;

  // Intent action used to launch Android apps.
  static const char kIntentAction[];

  // Extension IDs for the development and released versions of the Google Keep
  // Chrome app.
  static const char kDevKeepExtensionId[];
  static const char kProdKeepExtensionId[];
  // Web app IDs for testing and development versions of note-taking web apps.
  static const char kNoteTakingWebAppIdTest[];
  static const char kNoteTakingWebAppIdDev[];

  // Names of histograms.
  static const char kPreferredLaunchResultHistogramName[];
  static const char kDefaultLaunchResultHistogramName[];

  static void Initialize();
  static void Shutdown();
  static NoteTakingHelper* Get();

  NoteTakingHelper(const NoteTakingHelper&) = delete;
  NoteTakingHelper& operator=(const NoteTakingHelper&) = delete;

  bool play_store_enabled() const { return play_store_enabled_; }
  bool android_apps_received() const { return android_apps_received_; }

  void set_launch_chrome_app_callback_for_test(
      const LaunchChromeAppCallback& callback) {
    launch_chrome_app_callback_ = callback;
  }

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Inform the NoteTakingHelper that the given app was updated. May trigger
  // notifications to observers.
  void NotifyAppUpdated(Profile* profile, const std::string& app_id);

  // Returns a list of available note-taking apps, in the order they should be
  // shown in UI.
  std::vector<NoteTakingAppInfo> GetAvailableApps(Profile* profile);

  // Returns the ID of the preferred note-taking app. Empty if uninstalled or
  // not set.
  std::string GetPreferredAppId(Profile* profile);

  // Sets the preferred note-taking app. |app_id| is a value from a
  // NoteTakingAppInfo object.
  void SetPreferredApp(Profile* profile, const std::string& app_id);

  // Sets whether the preferred note taking app is allowed to run on the lock
  // screen.
  // Returns whether the app status changed.
  bool SetPreferredAppEnabledOnLockScreen(Profile* profile, bool enabled);

  // Returns true if an app that can be used to take notes is available. UI
  // surfaces that call LaunchAppForNewNote() should be hidden otherwise.
  bool IsAppAvailable(Profile* profile);

  // Launches the note-taking app to create a new note. IsAppAvailable() must
  // be called first.
  void LaunchAppForNewNote(Profile* profile);

  // arc::ArcIntentHelperObserver:
  void OnIntentFiltersUpdated(
      const std::optional<std::string>& package_name) override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  NoteTakingControllerClient* GetNoteTakingControllerClientForTesting() {
    return note_taking_controller_client_.get();
  }

 private:
  NoteTakingHelper();
  ~NoteTakingHelper() override;

  // Queries and returns the app IDs of note-taking Chrome/web apps that are
  // installed, enabled, and allowed for |profile|.
  std::vector<std::string> GetNoteTakingAppIds(Profile* profile) const;

  // Requests a list of Android note-taking apps from ARC.
  void UpdateAndroidApps();

  // Handles ARC's response to an earlier UpdateAndroidApps() call.
  void OnGotAndroidApps(std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

  // Helper method that launches |app_id| (either an Android package name or a
  // Chrome extension ID) to create a new note. Returns the attempt's result.
  LaunchResult LaunchAppInternal(Profile* profile, const std::string& app_id);

  // apps::AppRegistryCache::Observer
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // True iff Play Store is enabled (i.e. per the checkbox on the settings
  // page). Note that ARC may not be fully started yet when this is true, but it
  // is expected to start eventually. Similarly, ARC may not be fully shut down
  // yet when this is false, but will be eventually.
  bool play_store_enabled_ = false;

  // This is set to true after |android_apps_| is updated.
  bool android_apps_received_ = false;

  // Callback used to launch Chrome apps. Can be overridden for tests.
  LaunchChromeAppCallback launch_chrome_app_callback_;

  // IDs of allowed (but not necessarily installed) Chrome apps or web apps for
  // note-taking, in the order in which they're chosen if the user hasn't
  // expressed a preference. Explicitly set by command-line and a default
  // hard-coded list.
  std::vector<std::string> force_allowed_app_ids_;

  // Cached information about available Android note-taking apps.
  std::vector<NoteTakingAppInfo> android_apps_;

  // Observes ArcIntentHelper for changes to Android intent filters.
  // TODO(crbug.com/40228788): Remove when App Service publishes Android Apps
  // with note-taking intent.
  base::ScopedMultiSourceObservation<arc::ArcIntentHelperBridge,
                                     arc::ArcIntentHelperObserver>
      arc_intent_helper_observations_{this};

  // Obseves App Registry for all profiles with an App Registry.
  base::ScopedMultiSourceObservation<apps::AppRegistryCache,
                                     apps::AppRegistryCache::Observer>
      app_registry_observations_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<NoteTakingControllerClient> note_taking_controller_client_;

  base::WeakPtrFactory<NoteTakingHelper> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTE_TAKING_NOTE_TAKING_HELPER_H_
