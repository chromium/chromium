// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"

class BackgroundModeOptimizer;
class Browser;
class PrefRegistrySimple;
class Profile;
class ScopedProfileKeepAlive;
class StatusIcon;
class StatusTray;

namespace base {
class CommandLine;
}

namespace extensions {
class Extension;
}

using CommandIdHandlerVector = std::vector<base::RepeatingClosure>;

// BackgroundModeManager is responsible for switching Chrome into and out of
// "background mode" and for providing UI for the user to exit Chrome when there
// are no open browser windows.
//
// Chrome enters background mode whenever there is an application with the
// "background" permission installed. This class monitors the set of
// installed/loaded extensions to ensure that Chrome enters/exits background
// mode at the appropriate time.
//
// When Chrome is in background mode, it will continue running even after the
// last browser window is closed, until the user explicitly exits the app.
// Additionally, when in background mode, Chrome will launch on OS login with
// no open windows to allow apps with the "background" permission to run in the
// background.
class BackgroundModeManager : public BrowserListObserver,
                              public BackgroundApplicationListModel::Observer,
                              public ProfileAttributesStorage::Observer,
                              public StatusIconMenuModel::Delegate {
 public:
  BackgroundModeManager(const base::CommandLine& command_line,
                        ProfileAttributesStorage* profile_storage);

  BackgroundModeManager(const BackgroundModeManager&) = delete;
  BackgroundModeManager& operator=(const BackgroundModeManager&) = delete;

  ~BackgroundModeManager() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Adds an entry for |profile| to |background_mode_data_|, and starts tracking
  // events for this profile.
  void RegisterProfile(Profile* profile);

  // Removes the entry for |profile| from |background_mode_data_|, if present.
  // Returns true if a removal was performed.
  bool UnregisterProfile(Profile* profile);

  static void LaunchBackgroundApplication(
      Profile* profile,
      const extensions::Extension* extension);

  // Gets a browser window for |profile| associated with the active desktop.
  // Opens a new browser window if there isn't one for the active desktop.
  static Browser* GetBrowserWindowForProfile(Profile* profile);

  // Getter and setter for the flag indicating whether Chrome should start in
  // background mode the next time.
  static bool should_restart_in_background() {
    return should_restart_in_background_;
  }

  static void set_should_restart_in_background(bool enable) {
    should_restart_in_background_ = enable;
  }

  // Returns true if background mode is active.
  virtual bool IsBackgroundModeActive();

  // Returns true if we are in pure background mode, without windows.
  bool IsBackgroundWithoutWindows() const;

  // Suspends background mode until either ResumeBackgroundMode is called or
  // Chrome is restarted. This has the same effect as ending background mode
  // for the current browser session.
  virtual void SuspendBackgroundMode();

  // Resumes background mode. This ends a suspension of background mode, but
  // will not start it if it is not enabled.
  virtual void ResumeBackgroundMode();

  // For testing purposes.
  size_t NumberOfBackgroundModeData();

  int client_installed_notifications_for_test() {
    return client_installed_notifications_;
  }

 private:
  friend class AppBackgroundPageApiTest;
  friend class BackgroundModeManagerTest;
  friend class BackgroundModeManagerWithExtensionsTest;
  friend class AdvancedTestBackgroundModeManager;
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppLoadUnload);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundLaunchOnStartup);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundModeDisabledPreventsKeepAliveOnStartup);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           DisableBackgroundModeUnderTestFlag);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           EnableAfterBackgroundAppInstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           MultiProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ProfileAttributesStorage);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ProfileAttributesStorageObserver);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           DeleteBackgroundProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ForceInstalledExtensionsKeepAlive);
  FRIEND_TEST_ALL_PREFIXES(
      BackgroundModeManagerTest,
      ForceInstalledExtensionsKeepAliveReleasedOnAppTerminating);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BackgroundMenuGeneration);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BackgroundMenuGenerationMultipleProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerWithExtensionsTest,
                           BalloonDisplay);
  FRIEND_TEST_ALL_PREFIXES(BackgroundAppBrowserTest,
                           ReloadBackgroundApp);

  // Manages the background clients and menu items for a single profile. A
  // client is an extension.
  class BackgroundModeData : public StatusIconMenuModel::Delegate,
                             public extensions::ForceInstalledTracker::Observer,
                             public ProfileObserver {
   public:
    BackgroundModeData(BackgroundModeManager* manager,
                       Profile* profile,
                       CommandIdHandlerVector* command_id_handler_vector);
    ~BackgroundModeData() override;

    void SetTracker(extensions::ForceInstalledTracker* tracker);

    // Overrides from extensions::ForceInstalledTracker::Observer.
    void OnForceInstalledExtensionsReady() override;

    // Overrides from StatusIconMenuModel::Delegate implementation.
    void ExecuteCommand(int command_id, int event_flags) override;

    BackgroundApplicationListModel* applications() {
      return applications_.get();
    }

    // Returns a browser window, or creates one if none are open. Used by
    // operations (like displaying the preferences dialog) that require a
    // Browser window.
    Browser* GetBrowserWindow();

    // Returns if this profile has persistent background clients. A client is an
    // extension.
    bool HasPersistentBackgroundClient() const;

    // Returns if this profile has any background clients. A client is an
    // extension.
    bool HasAnyBackgroundClient() const;

    // Builds the profile specific parts of the menu. The menu passed in may
    // be a submenu in the case of multi-profiles or the main menu in the case
    // of the single profile case. If containing_menu is valid, we will add
    // menu as a submenu to it.
    void BuildProfileMenu(StatusIconMenuModel* menu,
                          StatusIconMenuModel* containing_menu);

    // Set the name associated with this background mode data for displaying in
    // the status tray.
    void SetName(const std::u16string& new_profile_name);

    // The name associated with this background mode data. This should match
    // the name in the ProfileAttributesStorage for this profile.
    std::u16string name();

    // Used for sorting BackgroundModeData*s.
    static bool BackgroundModeDataCompare(const BackgroundModeData* bmd1,
                                          const BackgroundModeData* bmd2);

    // Returns the set of new background apps (apps that have been loaded since
    // the last call to GetNewBackgroundApps()).
    std::set<const extensions::Extension*> GetNewBackgroundApps();

    // Acquires or releases a strong ref to the Profile, preventing/allowing it
    // to be deleted.
    //
    // Acquires the ref if background mode is active, and this profile has
    // persistent background apps. Releases it otherwise.
    void UpdateProfileKeepAlive();

    // ProfileObserver overrides:
    void OnProfileWillBeDestroyed(Profile* profile) override;

   private:
    const raw_ptr<BackgroundModeManager> manager_;

    base::ScopedObservation<Profile, ProfileObserver> profile_observation_{
        this};
    base::ScopedObservation<extensions::ForceInstalledTracker,
                            extensions::ForceInstalledTracker::Observer>
        force_installed_tracker_observation_{this};

    // The cached list of BackgroundApplications.
    std::unique_ptr<BackgroundApplicationListModel> applications_;

    // Name associated with this profile which is used to label its submenu.
    std::u16string name_;

    // The profile associated with this background app data.
    raw_ptr<Profile> profile_;

    // Prevents |profile_| from being deleted. Created or reset by
    // UpdateProfileKeepAlive().
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

    // Weak ref vector owned by BackgroundModeManager where the indices
    // correspond to Command IDs and values correspond to their handlers.
    const raw_ptr<CommandIdHandlerVector> command_id_handler_vector_;

    // The list of notified extensions for this profile. We track this to ensure
    // that we never notify the user about the same extension twice in a single
    // browsing session - this is done because the extension subsystem is not
    // good about tracking changes to the background permission around
    // extension reloads, and will sometimes report spurious permission changes.
    std::set<extensions::ExtensionId> current_extensions_;
  };

  using BackgroundModeInfoMap =
      std::map<const Profile*, std::unique_ptr<BackgroundModeData>>;

  void OnAppTerminating();

  // Called when ExtensionSystem is ready.
  void OnExtensionsReady(Profile* profile);

  // Called when the kBackgroundModeEnabled preference changes.
  void OnBackgroundModeEnabledPrefChanged();

  // BackgroundApplicationListModel::Observer implementation.
  void OnApplicationDataChanged() override;
  void OnApplicationListChanged(const Profile* profile) override;

  // Overrides from ProfileAttributesStorage::Observer
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

  // Overrides from StatusIconMenuModel::Delegate implementation.
  void ExecuteCommand(int command_id, int event_flags) override;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

  // Enables or disables background mode as needed, taking into account the
  // number of background clients. Updates the background status of |profile| in
  // the ProfileAttributesStorage if needed. If |new_client_names| is not empty
  // the user will be notified about the added client(s).
  void OnClientsChanged(const Profile* profile,
                        const std::vector<std::u16string>& new_client_names);

  // Invoked when a background client is installed so we can ensure that
  // launch-on-startup is enabled if appropriate.
  void OnBackgroundClientInstalled(const std::u16string& name);

  // Update whether Chrome should be launched on startup, depending on whether
  // |this| has any persistent background clients.
  void UpdateEnableLaunchOnStartup();

  // Called to make sure that our launch-on-startup mode is properly set.
  // (virtual so it can be mocked in tests).
  virtual void EnableLaunchOnStartup(bool should_launch);

  // Invoked when a client is installed so we can display a platform-specific
  // notification.
  virtual void DisplayClientInstalledNotification(const std::u16string& name);

  // Invoked to put Chrome in KeepAlive mode - chrome runs in the background
  // and has a status bar icon.
  void StartBackgroundMode();

  // Invoked to take Chrome out of KeepAlive mode - Chrome stops running in
  // the background and removes its status bar icon.
  void EndBackgroundMode();

  // Enables keep alive and the status tray icon if and only if background mode
  // is active and not suspended.
  virtual void UpdateKeepAliveAndTrayIcon();

  // Release keep_alive_for_startup_. This is invoked as a callback to make
  // make sure the message queue was initialized before we attempt to exit.
  void ReleaseStartupKeepAliveCallback();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for apps to load. This is called when we no
  // longer need to do this (either because the user has chosen to exit chrome
  // manually, or all apps have been loaded).
  void ReleaseStartupKeepAlive();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for force-installed extensions to install.
  // This is called when we no longer need to do this (either because the user
  // has chosen to exit chrome manually, or all force-installed extensions have
  // installed/failed installing).
  void ReleaseForceInstalledExtensionsKeepAlive();

  // Create a status tray icon to allow the user to shutdown Chrome when running
  // in background mode. Virtual to enable testing.
  virtual void CreateStatusTrayIcon();

  // Removes the status tray icon because we are exiting background mode.
  // Virtual to enable testing.
  virtual void RemoveStatusTrayIcon();

  // Create a context menu, or replace/update an existing context menu, for the
  // status tray icon which, among other things, allows the user to shutdown
  // Chrome when running in background mode. All profiles are listed under
  // the one context menu.
  virtual void UpdateStatusTrayIconContextMenu();

  // Returns the BackgroundModeData associated with this profile. If it does
  // not exist, returns NULL.
  BackgroundModeData* GetBackgroundModeData(const Profile* profile) const;

  // Returns the iterator associated with a particular profile name.
  // This should not be used to iterate over the background mode data. It is
  // used to efficiently delete an item from the background mode data map.
  BackgroundModeInfoMap::iterator GetBackgroundModeIterator(
      const std::u16string& profile_name);

  // Returns true if the "Let chrome run in the background" pref is checked.
  // (virtual to allow overriding in tests).
  virtual bool IsBackgroundModePrefEnabled() const;

  // Turns off background mode if it's currently enabled.
  void DisableBackgroundMode();

  // Turns on background mode if it's currently disabled.
  void EnableBackgroundMode();

  // Returns if any profile on the system has a persistent background client.
  // A client is an extension. (virtual to allow overriding in unit tests)
  virtual bool HasPersistentBackgroundClient() const;

  // Returns if any profile on the system has any background client.
  // A client is an extension. (virtual to allow overriding in unit tests)
  virtual bool HasAnyBackgroundClient() const;

  // Returns if there are persistent background clients for a profile. A client
  // is an extension.
  virtual bool HasPersistentBackgroundClientForProfile(
      const Profile* profile) const;

  // Returns true if we should be in background mode.
  bool ShouldBeInBackgroundMode() const;

  // Finds the BackgroundModeData associated with the last active profile,
  // if the profile isn't locked. Returns NULL otherwise.
  BackgroundModeData* GetBackgroundModeDataForLastProfile() const;

  // Creates sequenced task runner for making startup/login configuration
  // changes that may require file system or registry access.
  // The implementation of this function is platform specific and may return
  // a null scoped_refptr if the task runner isn't needed on that particular
  // platform.
  static scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();

  // Set to true when the next restart should be done in background mode.
  // Static because its value is read after the background mode manager is
  // destroyed.
  static bool should_restart_in_background_;

  // Reference to the ProfileAttributesStorage. It is used to update the
  // background app status of profiles when they open/close background apps.
  raw_ptr<ProfileAttributesStorage, AcrossTasksDanglingUntriaged>
      profile_storage_;

  // Registrars for managing our change observers.
  base::CallbackListSubscription on_app_terminating_subscription_;
  PrefChangeRegistrar pref_registrar_;

  // The profile-keyed data for this background mode manager. Keyed on profile.
  BackgroundModeInfoMap background_mode_data_;

  // Indexes the command ids for the entire background menu to their handlers.
  CommandIdHandlerVector command_id_handler_vector_;

  // Maintains submenu lifetime for the multiple profile context menu.
  std::vector<std::unique_ptr<StatusIconMenuModel>> submenus;

  // Reference to our status tray. If null, the platform doesn't support status
  // icons.
  raw_ptr<StatusTray, DanglingUntriaged> status_tray_ = nullptr;

  // Reference to our status icon (if any) - owned by the StatusTray.
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_ = nullptr;

  // Reference to our status icon's context menu (if any) - owned by the
  // status_icon_.
  raw_ptr<StatusIconMenuModel, DanglingUntriaged> context_menu_ = nullptr;

  // Set to true when we are running in background mode. Allows us to track our
  // current background state so we can take the appropriate action when the
  // user disables/enables background mode via preferences.
  bool in_background_mode_ = false;

  // Background mode does not always keep Chrome alive. When it does, it is
  // using this scoped object.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Set when we are keeping chrome running during the startup process - this
  // is required when running with the --no-startup-window flag, as otherwise
  // chrome would immediately exit due to having no open windows. Resets
  // after |ExtensionSystem| startup.
  std::unique_ptr<ScopedKeepAlive> keep_alive_for_startup_;

  // Like |keep_alive_for_startup_|, but resets after force-installed
  // extensions are finished installing.
  std::unique_ptr<ScopedKeepAlive> keep_alive_for_force_installed_extensions_;

  // Reference to the optimizer to use to reduce Chrome's footprint when in
  // background mode. If null, optimizations are disabled.
  std::unique_ptr<BackgroundModeOptimizer> optimizer_;

  // Set to true when Chrome is running with the --keep-alive-for-test flag
  // (used for testing background mode without having to install a background
  // app).
  bool keep_alive_for_test_ = false;

  // Tracks the number of "background app installed" notifications shown to the
  // user. Used for testing.
  int client_installed_notifications_ = 0;

  // Set to true when background mode is suspended.
  bool background_mode_suspended_ = false;

  std::optional<bool> launch_on_startup_enabled_;

  // Task runner for making startup/login configuration changes that may
  // require file system or registry access.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BackgroundModeManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
