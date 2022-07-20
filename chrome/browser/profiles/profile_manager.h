// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/common/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_list_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class AccountProfileMapper;
#endif

class ProfileAttributesStorage;
enum class ProfileKeepAliveOrigin;
class ProfileManagerObserver;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

// Manages the lifecycle of Profile objects.
//
// Note that the Profile objects may be destroyed when their last browser window
// is closed. The DestroyProfileOnBrowserClose flag controls this behavior.
class ProfileManager : public Profile::Delegate {
 public:
  using CreateCallback =
      base::RepeatingCallback<void(Profile*, Profile::CreateStatus)>;
  using ProfileLoadedCallback = base::OnceCallback<void(Profile*)>;

  explicit ProfileManager(const base::FilePath& user_data_dir);
  ProfileManager(const ProfileManager&) = delete;
  ProfileManager& operator=(const ProfileManager&) = delete;
  ~ProfileManager() override;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();
#endif

  // Physically remove deleted profile directories from disk.
  static void NukeDeletedProfilesFromDisk();

  // Get the Profile last used (the Profile to which owns the most recently
  // focused window) with this Chrome build. If no signed profile has been
  // stored in Local State, hand back the Default profile.
  // If the Profile is going to be used to open a new window then consider using
  // GetLastUsedProfileAllowedByPolicy() instead.
  // Except in ChromeOS guest sessions, the returned profile is always a regular
  // profile (non-OffTheRecord).
  // WARNING: if the profile does not exist, this function creates it
  // synchronously, causing blocking file I/O. Use GetLastUsedProfileIfLoaded()
  // to avoid creating the profile synchronously.
  static Profile* GetLastUsedProfile();

  // Same as GetLastUsedProfile() but returns nullptr if the profile is not
  // loaded. Does not block.
  static Profile* GetLastUsedProfileIfLoaded();

  // Same as `GetLastUsedProfile()` but returns the incognito `Profile` if
  // incognito mode is forced. This should be used if the last used `Profile`
  // will be used to open new browser windows.
  // WARNING: if the `Profile` does not exist, this function creates it
  // synchronously, causing blocking file I/O. Use
  // `LoadLastUsedProfileAllowedByPolicy()` instead.
  static Profile* GetLastUsedProfileAllowedByPolicy();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Same as `GetLastUsedProfileAllowedByPolicy()`, but asynchronously loads the
  // `Profile` if it's not already loaded.
  // TODO(https://crbug.com/1176734): Implement on Ash. Requires handling the
  // cases where the user is not logged in, and implementing an asynchronous
  // version of `GetActiveUserOrOffTheRecordProfile()`.
  static void LoadLastUsedProfileAllowedByPolicy(
      ProfileLoadedCallback callback);
#endif

  // Helper function that returns the OffTheRecord profile if it is forced for
  // `profile` (normal mode is not available for browsing).
  // Returns nullptr if `profile` is nullptr.
  static Profile* MaybeForceOffTheRecordMode(Profile* profile);

  // Get the Profiles which are currently open, i.e. have open browsers or were
  // open the last time Chrome was running. Profiles that fail to initialize are
  // skipped. The Profiles appear in the order they were opened. The last used
  // profile will be on the list if it is initialized successfully, but its
  // index on the list will depend on when it was opened (it is not necessarily
  // the last one).
  // Note: The list returned might contain on-the-record irregular profiles
  // like the System profile.
  static std::vector<Profile*> GetLastOpenedProfiles();

  // WARNING: do not use this function on Desktop platforms (Windows, Mac,
  // Linux). See https://crbug.com/1264436 for more info.
  // TODO(https://crbug.com/1264436): restrict this function to Android and
  // ChromeOS.
  //
  // Get the profile for the user which created the current session.
  // Note that in case of a guest account this will return a 'suitable' profile.
  static Profile* GetPrimaryUserProfile();

  // WARNING: do not use this function on Desktop platforms (Windows, Mac,
  // Linux). See https://crbug.com/1264436 for more info.
  // TODO(https://crbug.com/1264436): restrict this function to Android and
  // ChromeOS.
  //
  // Get the profile for the currently active user.
  // Note that in case of a guest account this will return a 'suitable' profile.
  static Profile* GetActiveUserProfile();

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  // Load and return the initial profile for browser. On ChromeOS, this returns
  // either the sign-in profile or the active user profile depending on whether
  // browser is started normally or is restarted after crash. On other
  // platforms, this returns the default profile.
  static Profile* CreateInitialProfile();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

  void AddObserver(ProfileManagerObserver* observer);
  void RemoveObserver(ProfileManagerObserver* observer);

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  // Because this method might synchronously create a new profile, it should
  // only be called for the initial profile or in tests, where blocking is
  // acceptable. Returns null if creation of the new profile fails.
  // TODO(bauerb): Migrate calls from other code to GetProfileByPath(), then
  // make this method private.
  Profile* GetProfile(const base::FilePath& profile_dir);

  // Returns regular or off-the-record profile given its profile key.
  static Profile* GetProfileFromProfileKey(ProfileKey* profile_key);

  // Returns total number of profiles available on this machine.
  size_t GetNumberOfProfiles();

  // Asynchronously loads an existing profile given its |profile_base_name|
  // (which is the directory name within the user data directory), optionally in
  // |incognito| mode. The |callback| will be called with the Profile when it
  // has been loaded, or with a nullptr otherwise.
  // Should be called on the UI thread.
  // Unlike CreateProfileAsync this will not create a profile if one doesn't
  // already exist on disk
  // Returns true if the profile exists, but the final loaded profile will come
  // as part of the callback.
  bool LoadProfile(const base::FilePath& profile_base_name,
                   bool incognito,
                   ProfileLoadedCallback callback);
  bool LoadProfileByPath(const base::FilePath& profile_path,
                         bool incognito,
                         ProfileLoadedCallback callback);

  // Explicit asynchronous creation of a profile located at |profile_path|.
  // If the profile has already been created then callback is called
  // immediately. Should be called on the UI thread.
  void CreateProfileAsync(const base::FilePath& profile_path,
                          const CreateCallback& callback);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(const void* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use.
  base::FilePath GetInitialProfileDir();

  // Get the path of the last used profile, or if that's undefined, the default
  // profile.
  base::FilePath GetLastUsedProfileDir();

  // Returns the path of a profile with the requested account, or the empty
  // path if none exists.
  base::FilePath GetProfileDirForEmail(const std::string& email);

  // Returns created and fully initialized profiles. Notes:
  // - profiles order is NOT guaranteed to be related with the creation order.
  // - only returns profiles owned by the ProfileManager. In particular, this
  //   does not return incognito profiles, because they are owned by their
  //   original profiles.
  // - may also return irregular profiles like on-the-record System or Guest
  //   profiles.
  std::vector<Profile*> GetLoadedProfiles() const;

  // If a profile with the given path is currently managed by this object and
  // fully initialized, return a pointer to the corresponding Profile object;
  // otherwise return null.
  Profile* GetProfileByPath(const base::FilePath& path) const;

  // Asynchronously creates a new profile in the next available multiprofile
  // directory. Directories are named "profile_1", "profile_2", etc., in
  // sequence of creation. (Because directories can be removed, however, it may
  // be the case that at some point the list of numbered profiles is not
  // continuous.) |callback| may be invoked multiple times (for
  // CREATE_STATUS_INITIALIZED and CREATE_STATUS_CREATED) so binding parameters
  // with bind::Passed() is prohibited. If |is_hidden| is true, the new profile
  // will be created as ephemeral (removed on the next startup) and omitted (not
  // visible in the list of profiles).
  static void CreateMultiProfileAsync(const std::u16string& name,
                                      size_t icon_index,
                                      bool is_hidden,
                                      const CreateCallback& callback);

  // Returns the full path to be used for guest profiles.
  static base::FilePath GetGuestProfilePath();

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  // Returns the full path to be used for system profiles.
  static base::FilePath GetSystemProfilePath();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns the full path of the primary profile on lacros.
  static base::FilePath GetPrimaryUserProfilePath();
#endif

  // Get the path of the next profile directory and increment the internal
  // count.
  // Lack of side effects:
  // This function doesn't actually create the directory or touch the file
  // system.
  base::FilePath GenerateNextProfileDirectoryPath();

  // Returns a ProfileAttributesStorage object which can be used to get
  // information about profiles without having to load them from disk.
  ProfileAttributesStorage& GetProfileAttributesStorage();

  // Returns a ProfileShortcut Manager that enables the caller to create
  // profile specfic desktop shortcuts.
  ProfileShortcutManager* profile_shortcut_manager();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  AccountProfileMapper* GetAccountProfileMapper();
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Less strict version of ScheduleProfileForDeletion(), silently exits if
  // profile is either scheduling or marked for deletion.
  void MaybeScheduleProfileForDeletion(
      const base::FilePath& profile_dir,
      ProfileLoadedCallback callback,
      ProfileMetrics::ProfileDelete deletion_source);

  // Schedules the profile at the given path to be deleted on shutdown. If we're
  // deleting the last profile, a new one will be created in its place, and in
  // that case the callback will be called when profile creation is complete.
  void ScheduleProfileForDeletion(const base::FilePath& profile_dir,
                                  ProfileLoadedCallback callback);

  // Schedules the ephemeral profile at the given path to be deleted on
  // shutdown. New profiles will not be created.
  void ScheduleEphemeralProfileForDeletion(const base::FilePath& profile_dir);

  // Deletes Guest profile's browsing data.
  static void CleanUpGuestProfile();
#endif

  // Returns if profile is marked for deletion.
  static bool IsProfileDirectoryMarkedForDeletion(
      const base::FilePath& profile_dir);

  // Autoloads profiles if they are running background apps.
  void AutoloadProfiles();

  // Checks if any ephemeral profiles are left behind (e.g. because of a browser
  // crash) and schedule them for deletion.
  void CleanUpEphemeralProfiles();

  // Checks if files of deleted profiles are left behind (e.g. because of a
  // browser crash) and delete them in case they still exist.
  void CleanUpDeletedProfiles();

  // Initializes user prefs of |profile|. This includes profile name and
  // avatar values.
  void InitProfileUserPrefs(Profile* profile);

  // Register and add testing profile to the ProfileManager. Use ONLY in tests.
  // This allows the creation of Profiles outside of the standard creation path
  // for testing. If |addToStorage|, adds to ProfileAttributesStorage as well.
  // Use ONLY in tests.
  void RegisterTestingProfile(std::unique_ptr<Profile> profile,
                              bool add_to_storage);

  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  // Profile::Delegate implementation:
  void OnProfileCreationStarted(Profile* profile,
                                Profile::CreateMode create_mode) override;
  void OnProfileCreationFinished(Profile* profile,
                                 Profile::CreateMode create_mode,
                                 bool success,
                                 bool is_new_profile) override;

  // Used for testing. Returns true if |profile| has at least one ref of type
  // |origin|.
  bool HasKeepAliveForTesting(const Profile* profile,
                              ProfileKeepAliveOrigin origin);

  // Disables the periodic reporting of profile metrics, as this is causing
  // tests to time out.
  void DisableProfileMetricsForTesting();

  // Returns the number of profiles in a "zombie" state, which means either:
  //
  //   - this profile was destroyed from memory,
  //   - this profile has a refcount of 0, meaning it's safe to destroy.
  //
  // Looks at the list of profiles that were loaded during this browsing
  // session, to determine if they're all still loaded in memory and look at
  // their refcount.
  //
  // This is used for an A/B test, that measures the impact of the
  // DestroyProfileOnBrowserClose variation on memory usage.
  size_t GetZombieProfileCount() const;

  // Returns ProfileKeepAliveOrigin map for the profile associated with given
  // |path|, if the profile is registered. Otherwise returns an empty map.
  std::map<ProfileKeepAliveOrigin, int> GetKeepAlivesByPath(
      const base::FilePath& path);

 protected:
  // Creates a new profile by calling into the profile's profile creation
  // method. Virtual so that unittests can return a TestingProfile instead
  // of the Profile's result. Returns null if creation fails.
  virtual std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path);

  // Creates a new profile asynchronously by calling into the profile's
  // asynchronous profile creation method. Virtual so that unittests can return
  // a TestingProfile instead of the Profile's result.
  virtual std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path);

  void set_do_final_services_init(bool do_final_services_init) {
    do_final_services_init_ = do_final_services_init;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetAccountProfileMapperForTests(
      std::unique_ptr<AccountProfileMapper> mapper);
#endif

 private:
  friend class TestingProfileManager;
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, DeleteAllProfiles);
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, SwitchToProfile);
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerTest, ScopedProfileKeepAlive);

  // For AddKeepAlive() and RemoveKeepAlive().
  friend class ScopedProfileKeepAlive;

  // This class contains information about profiles which are being loaded or
  // were loaded.
  class ProfileInfo {
   public:
    ProfileInfo(const ProfileInfo&) = delete;
    ProfileInfo& operator=(const ProfileInfo&) = delete;
    ~ProfileInfo();

    // Returns a non-created ProfileInfo that does not own |profile|.
    static std::unique_ptr<ProfileInfo> FromUnownedProfile(Profile* profile);

    // Takes ownership of |profile|, so it gets destroyed when this ProfileInfo
    // is deleted.
    void TakeOwnershipOfProfile(std::unique_ptr<Profile> profile);

    // Marks the Profile as created, so GetCreatedProfile() returns non-null.
    void MarkProfileAsCreated(Profile* profile);

    // Returns the owned Profile, if creation is complete (i.e., prefs are
    // loaded). Returns null otherwise.
    Profile* GetCreatedProfile() const;

    // Returns the Profile, regardless of whether it's owned/unowned or whether
    // prefs are loaded.
    Profile* GetRawProfile() const;

    // TODO(nicolaso): Make |keep_alives| and |callbacks| private with
    // accessors.

    // Strong references to this Profile (e.g. a Browser object, a
    // BackgroundModeManager, ...)
    //
    // Initially contains a kWaitingForFirstBrowserWindow entry, which gets
    // removed when a kBrowserWindow keepalive is added.
    std::map<ProfileKeepAliveOrigin, int> keep_alives;
    // List of callbacks to run when profile initialization is done. Note, when
    // profile is fully loaded this vector will be empty.
    std::vector<CreateCallback> callbacks;

   private:
    // Callers should use FromOwned/UnownedProfile() instead.
    ProfileInfo();

    // The Profile pointed to by this ProfileInfo.
    raw_ptr<Profile> unowned_profile_ = nullptr;

    // For when the Profile is owned, via FromOwnedProfile() or
    // TakeOwnershipOfProfile().
    std::unique_ptr<Profile> owned_profile_;

    // Whether profile has been fully loaded (created and initialized). See
    // MarkProfileAsCreated().
    bool created_ = false;
  };

  // Increments/decrements the refcount on a |profile|. (it must not be an
  // off-the-record profile)
  void AddKeepAlive(const Profile* profile, ProfileKeepAliveOrigin origin);
  void RemoveKeepAlive(const Profile* profile, ProfileKeepAliveOrigin origin);

  void RecordZombieMetrics();

  // Removes the kWaitingForFirstBrowserWindow keepalive. This allows a
  // Profile* to be deleted from now on, even if it never had a visible
  // browser window.
  void ClearFirstBrowserWindowKeepAlive(const Profile* profile);

  // Helper for RemoveKeepAlive() and ClearFirstBrowserWindowFlag(). If the
  // refcount to this Profile is zero, calls RemoveKeepAlive().
  void DeleteProfileIfNoKeepAlive(const ProfileInfo* info);

  // Does final initial actions.
  void DoFinalInit(ProfileInfo* profile_info, bool go_off_the_record);
  void DoFinalInitForServices(Profile* profile, bool go_off_the_record);
  void DoFinalInitLogging(Profile* profile);

  // Returns the profile of the active user and / or the off the record profile
  // if needed. This adds the profile to the ProfileManager if it doesn't
  // already exist. The method will return NULL if the profile doesn't exist
  // and we can't create it.
  // The profile used can be overridden by using --login-profile on cros.
  Profile* GetActiveUserOrOffTheRecordProfile();

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(std::unique_ptr<Profile> profile);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Removes the Profile at |profile_dir| from the manager and destroys it. If
  // it's an ephemeral profile, also nuke the |profile_dir| directory from disk
  // afterwards.
  void RemoveProfile(const base::FilePath& profile_dir);
#endif

  // Synchronously creates and returns a profile. This handles both the full
  // creation and adds it to the set managed by this ProfileManager. Returns
  // null if creation fails.
  Profile* CreateAndInitializeProfile(const base::FilePath& profile_dir);

#if !BUILDFLAG(IS_ANDROID)
  // Continues the scheduled profile deletion after closing all the profile's
  // browsers tabs. Creates a new profile if the profile to be deleted is the
  // last non-supervised profile. In the Mac, loads the next non-supervised
  // profile if the profile to be deleted is the active profile.
  void EnsureActiveProfileExistsBeforeDeletion(
      ProfileLoadedCallback callback,
      const base::FilePath& profile_dir);

  // Schedules the profile at the given path to be deleted on shutdown,
  // and marks the new profile as active.
  void FinishDeletingProfile(const base::FilePath& profile_dir,
                             const base::FilePath& new_active_profile_dir);
  void OnLoadProfileForProfileDeletion(const base::FilePath& profile_dir,
                                       Profile* profile);

  // Searches for the latest active profile that respects |predicate|, already
  // loaded preferably. Returns nullopt if no existing profile respects all the
  // conditions.
  absl::optional<base::FilePath> FindLastActiveProfile(
      base::RepeatingCallback<bool(ProfileAttributesEntry*)> predicate);
#endif

  // Registers profile with given info. Returns pointer to created ProfileInfo
  // entry.
  ProfileInfo* RegisterUnownedProfile(Profile* profile);
  ProfileInfo* RegisterOwnedProfile(std::unique_ptr<Profile> profile);

  // Returns ProfileInfo associated with given |path|, registered earlier with
  // RegisterProfile.
  ProfileInfo* GetProfileInfoByPath(const base::FilePath& path) const;

  // Returns a registered profile. In contrast to GetProfileByPath(), this will
  // also return a profile that is not fully initialized yet, so this method
  // should be used carefully.
  Profile* GetProfileByPathInternal(const base::FilePath& path) const;

  // Returns whether |path| is allowed for profile creation.
  bool IsAllowedProfilePath(const base::FilePath& path) const;

  // Whether a new profile can be created at |path|.
  bool CanCreateProfileAtPath(const base::FilePath& path) const;

  // Adds |profile| to the profile attributes storage if it hasn't been added
  // yet.
  void AddProfileToStorage(Profile* profile);

  // Apply settings for profiles created by the system rather than users: The
  // (desktop) Guest User profile and (desktop) System Profile.
  void SetNonPersonalProfilePrefs(Profile* profile);

  // Determines if profile should be OTR.
  bool ShouldGoOffTheRecord(Profile* profile);

  void RunCallbacks(const std::vector<CreateCallback>& callbacks,
                    Profile* profile,
                    Profile::CreateStatus status);

  void SaveActiveProfiles();

#if !BUILDFLAG(IS_ANDROID)
  void OnBrowserOpened(Browser* browser);
  void OnBrowserClosed(Browser* browser);

  // Updates the last active user of the current session.
  // On Chrome OS updating this user will have no effect since when browser is
  // restored after crash there's another preference that is taken into account.
  // See kLastActiveUser in UserManagerBase.
  void UpdateLastUser(Profile* last_active);

  class BrowserListObserver : public ::BrowserListObserver {
   public:
    explicit BrowserListObserver(ProfileManager* manager);
    BrowserListObserver(const BrowserListObserver&) = delete;
    BrowserListObserver& operator=(const BrowserListObserver&) = delete;
    ~BrowserListObserver() override;

    // ::BrowserListObserver implementation.
    void OnBrowserAdded(Browser* browser) override;
    void OnBrowserRemoved(Browser* browser) override;
    void OnBrowserSetLastActive(Browser* browser) override;

   private:
    raw_ptr<ProfileManager> profile_manager_;
  };

  // If the `loaded_profile` has been loaded successfully (according to
  // `status`) and isn't already scheduled for deletion, then finishes adding
  // `profile_to_delete_dir` to the queue of profiles to be deleted, and updates
  // the kProfileLastUsed preference based on
  // `last_non_supervised_profile_path`. `keep_alive` may be null and is used
  // to ensure shutdown does not start.
  void OnNewActiveProfileLoaded(
      const base::FilePath& profile_to_delete_path,
      const base::FilePath& last_non_supervised_profile_path,
      ProfileLoadedCallback* callback,
      ScopedKeepAlive* keep_alive,
      Profile* loaded_profile,
      Profile::CreateStatus status);

  void OnClosingAllBrowsersChanged(bool closing);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Destroy after |profile_attributes_storage_| since Profile destruction may
  // trigger some observers to unregister themselves.
  base::ObserverList<ProfileManagerObserver> observers_;

  // Object to cache various information about profiles. Contains information
  // about every profile which has been created for this instance of Chrome,
  // if it has not been explicitly deleted. It must be destroyed after
  // |profiles_info_| because ~ProfileInfo can trigger a chain of events leading
  // to an access to this member.
  std::unique_ptr<ProfileAttributesStorage> profile_attributes_storage_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Object that maintains a mapping between accounts known to the OS and Chrome
  // profiles. AccountProfileMapper has dependencies on other members of this
  // class. It must be destroyed after `profiles_info_` and before
  // `profile_attributes_storage_`.
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
#endif

  base::CallbackListSubscription closing_all_browsers_subscription_;

  // The path to the user data directory (DIR_USER_DATA).
  const base::FilePath user_data_dir_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_ = false;

#if !BUILDFLAG(IS_ANDROID)
  BrowserListObserver browser_list_observer_{this};
#endif  // !BUILDFLAG(IS_ANDROID)

  // Maps profile path to ProfileInfo (if profile has been created). Use
  // RegisterProfile() to add into this map. This map owns all loaded profile
  // objects in a running instance of Chrome.
  using ProfilesInfoMap =
      std::map<base::FilePath, std::unique_ptr<ProfileInfo>>;
  ProfilesInfoMap profiles_info_;

  // Manages the process of creating, deleteing and updating Desktop shortcuts.
  std::unique_ptr<ProfileShortcutManager> profile_shortcut_manager_;

  // For keeping track of the last active profiles.
  std::map<Profile*, int> browser_counts_;
  // On startup we launch the active profiles in the order they became active
  // during the last run. This is why they are kept in a list, not in a set.
  std::vector<Profile*> active_profiles_;
  bool closing_all_browsers_ = false;

  // Becomes true once the refcount for any profile hits 0. This is used to
  // measure how often DestroyProfileOnBrowserClose logic triggers.
  bool could_have_destroyed_profile_ = false;

  // Set of profile dirs that were loaded during this browsing session at some
  // point (or are currently loaded). This is used to measure memory savings
  // from DestroyProfileOnBrowserClose.
  //
  // Doesn't include the System and Guest profile paths.
  std::set<base::FilePath> ever_loaded_profiles_;

  // Runs a task every 30 minutes to record the number of zombie & non-zombie
  // profiles in memory.
  base::RepeatingTimer zombie_metrics_timer_;

  // Controls whether to initialize some services. Only disabled for testing.
  bool do_final_services_init_ = true;

  // TODO(chrome/browser/profiles/OWNERS): Usage of this in profile_manager.cc
  // should likely be turned into DCHECK_CURRENTLY_ON(BrowserThread::UI) for
  // consistency with surrounding code in the same file but that wasn't trivial
  // enough to do as part of the mass refactor CL which introduced
  // |thread_checker_|, ref. https://codereview.chromium.org/2907253003/#msg37.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ProfileManager> weak_factory_{this};
};

// Same as the ProfileManager, but doesn't initialize some services of the
// profile. This one is useful in unittests.
class ProfileManagerWithoutInit : public ProfileManager {
 public:
  explicit ProfileManagerWithoutInit(const base::FilePath& user_data_dir);
  ProfileManagerWithoutInit(const ProfileManagerWithoutInit&) = delete;
  ProfileManagerWithoutInit& operator=(const ProfileManagerWithoutInit&) =
      delete;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
