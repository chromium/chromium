// Copyright 2012 The Chromium Authors
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
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/ui/browser_list_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
class ProfileManagerAndroid;
#endif

class DeleteProfileHelper;
class ProfileAttributesStorage;
enum class ProfileKeepAliveOrigin;
class ProfileManagerObserver;
class ScopedProfileKeepAlive;

// Manages the lifecycle of Profile objects.
//
// Note that the Profile objects may be destroyed when their last browser window
// is closed. The DestroyProfileOnBrowserClose flag controls this behavior.
class ProfileManager : public Profile::Delegate {
 public:
  using ProfileLoadedCallback = base::OnceCallback<void(Profile*)>;

  explicit ProfileManager(const base::FilePath& user_data_dir);
  ProfileManager(const ProfileManager&) = delete;
  ProfileManager& operator=(const ProfileManager&) = delete;
  ~ProfileManager() override;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();
#endif

  // Get the `Profile` last used (the `Profile` which owns the most recently
  // focused window) with this Chrome build. If no signed profile has been
  // stored in Local State, hand back the Default profile.
  // If the profile is going to be used to open a new window then consider using
  // `GetLastUsedProfileAllowedByPolicy()` instead.
  // Except in ChromeOS guest sessions, the returned profile is always a regular
  // profile (non-OffTheRecord).
  // WARNING: if the profile is not loaded, this function loads it
  // synchronously, causing blocking file I/O. Use
  // `GetLastUsedProfileIfLoaded()` to avoid loading the profile synchronously.
  static Profile* GetLastUsedProfile();

  // Same as `GetLastUsedProfile()` but returns nullptr if the profile is not
  // loaded. Does not block.
  static Profile* GetLastUsedProfileIfLoaded();

  // Same as `GetLastUsedProfile()` but returns the incognito `Profile` if
  // incognito mode is forced. This should be used if the last used `Profile`
  // will be used to open new browser windows.
  // WARNING: if the `Profile` is not loaded, this function loads it
  // synchronously, causing blocking file I/O.
  static Profile* GetLastUsedProfileAllowedByPolicy();

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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // Get the profile for the user which created the current session.
  // Note that in case of a guest account this will return a 'suitable' profile.
  static Profile* GetPrimaryUserProfile();

  // Get the profile for the currently active user.
  // Note that in case of a guest account this will return a 'suitable' profile.
  static Profile* GetActiveUserProfile();
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // Load and return the initial profile for browser. On ChromeOS, this returns
  // either the sign-in profile or the active user profile depending on whether
  // browser is started normally or is restarted after crash. On other
  // platforms, this returns the default profile.
  static Profile* CreateInitialProfile();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

  void AddObserver(ProfileManagerObserver* observer);
  void RemoveObserver(ProfileManagerObserver* observer);

  // Returns a profile for a specific profile directory within the user data
  // dir. This will return an existing profile it had already been created,
  // otherwise it will create and manage it.
  // Because this method might synchronously load a new profile, it should
  // only be called for the initial profile or in tests, where blocking is
  // acceptable. Returns nullptr if loading the new profile fails.
  // TODO(bauerb): Migrate calls from other code to `GetProfileByPath()`, then
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

  // Creates or loads the profile located at |profile_path|.
  // Should be called on the UI thread.
  // Params:
  // - `initialized_callback`: called when profile initialization is done, will
  // return nullptr if failed. If the profile has already been fully loaded then
  // this callback is called immediately.
  // - `created_callback`: called when profile creation is done (default
  // implementation to do nothing).
  // Note: Refer to `Profile::CreateStatus` for the definition of CREATED and
  // INITIALIZED profile creation status.
  void CreateProfileAsync(
      const base::FilePath& profile_path,
      base::OnceCallback<void(Profile*)> initialized_callback,
      base::OnceCallback<void(Profile*)> created_callback = {});

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(const void* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use.
  base::FilePath GetInitialProfileDir();

  // Get the path of the last used profile, or if that's undefined, the default
  // profile.
  base::FilePath GetLastUsedProfileDir();
  static base::FilePath GetLastUsedProfileBaseName();

  // Returns the path of a profile with the requested account, or the empty
  // path if none exists.
  base::FilePath GetProfileDirForEmail(const std::string& email);

  // Returns loaded and fully initialized profiles. Notes:
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

#if !BUILDFLAG(IS_ANDROID)
  // Asynchronously creates a new profile in the next available multiprofile
  // directory. Directories are named "profile_1", "profile_2", etc., in
  // sequence of creation. (Because directories can be deleted, however, it may
  // be the case that at some point the list of numbered profiles is not
  // continuous.) If |is_hidden| is true, the new profile
  // will be created as ephemeral (deleted on the next startup) and omitted (not
  // visible in the list of profiles).
  // Params:
  // - `initialized_callback`: called when profile initialization is done, will
  // return nullptr if failed. If the profile has already been fully loaded then
  // this callback is called immediately.
  // - `created_callback`: called when profile creation is done (default
  // implementation to do nothing).
  // Note: Refer to `Profile::CreateStatus` for the definition of CREATED and
  // INITIALIZED profile creation status.
  static void CreateMultiProfileAsync(
      const std::u16string& name,
      size_t icon_index,
      bool is_hidden,
      base::OnceCallback<void(Profile*)> initialized_callback,
      base::OnceCallback<void(Profile*)> created_callback = {});
#endif  // !BUILDFLAG(IS_ANDROID)

  // Returns the full path to be used for guest profiles.
  static base::FilePath GetGuestProfilePath();

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  // Returns the full path to be used for system profiles.
  static base::FilePath GetSystemProfilePath();
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

  // Get the path of the next profile directory and increment the internal
  // count.
  // Lack of side effects:
  // This function doesn't actually create the directory or touch the file
  // system.
  base::FilePath GenerateNextProfileDirectoryPath();

  // Get the path of the next profile directory without incrementing the
  // internal count.
  // This function should only be used for path checking before
  // 'GenerateNextProfileDirectoryPath' as this will return the path generated
  // the next time `GenerateNextProfileDirectoryPath` is called.
  base::FilePath GetNextExpectedProfileDirectoryPath();

  // Returns a ProfileAttributesStorage object which can be used to get
  // information about profiles without having to load them from disk.
  ProfileAttributesStorage& GetProfileAttributesStorage();

  // Returns a ProfileShortcut Manager that enables the caller to create
  // profile specfic desktop shortcuts.
  ProfileShortcutManager* profile_shortcut_manager();

#if !BUILDFLAG(IS_ANDROID)
  // Searches for the latest active profile that respects |predicate|, already
  // loaded preferably. Returns nullopt if no existing profile respects all the
  // conditions.
  std::optional<base::FilePath> FindLastActiveProfile(
      base::RepeatingCallback<bool(ProfileAttributesEntry*)> predicate);

  DeleteProfileHelper& GetDeleteProfileHelper();
#endif

  // Autoloads profiles if they are running background apps.
  void AutoloadProfiles();

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

  // Removes the kWaitingForFirstBrowserWindow keepalive. This allows a
  // Profile* to be deleted from now on, even if it never had a visible
  // browser window.
  void ClearFirstBrowserWindowKeepAlive(const Profile* profile);

  // Returns whether |path| is allowed for profile creation.
  bool IsAllowedProfilePath(const base::FilePath& path) const;

  // Notifies `OnProfileMarkedForPermanentDeletion()` to the observers.
  void NotifyOnProfileMarkedForPermanentDeletion(Profile* profile);

  bool has_updated_last_opened_profiles() const {
    return has_updated_last_opened_profiles_;
  }

  // Sets the last-used profile to `last_active`, and also sets that profile's
  // last-active time to now. If the profile has a primary account, this also
  // sets its last-active time to now.
  // Public so that `ProfileManagerAndroid` can call it.
  void SetProfileAsLastUsed(Profile* last_active);

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
    // List of callbacks to run when profile initialization (success or fail) is
    // done. Note, when profile is fully loaded this vector will be empty.
    std::vector<ProfileLoadedCallback> init_callbacks;
    // List of callbacks to run when profile is created. Note, when
    // profile is fully loaded this vector will be empty.
    std::vector<ProfileLoadedCallback> created_callbacks;

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

  // Helper for `RemoveKeepAlive()` and `ClearFirstBrowserWindowFlag()`. If the
  // refcount to this `Profile` is zero, calls `UnloadProfile()`.
  void UnloadProfileIfNoKeepAlive(const ProfileInfo* info);

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Unloads the `Profile` at `profile_dir` from the manager and destroys the
  // `Profile` C++ object. If it's an ephemeral profile, also deletes the
  // profile permanently and nukes the `profile_dir` directory from disk.
  void UnloadProfile(const base::FilePath& profile_dir);
#endif

  // Synchronously creates and returns a profile. This handles both the full
  // creation and adds it to the set managed by this ProfileManager. Returns
  // null if creation fails.
  Profile* CreateAndInitializeProfile(
      const base::FilePath& profile_dir,
      base::OnceCallback<std::unique_ptr<Profile>(const base::FilePath&)>
          factory);

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

  void SaveActiveProfiles();

#if !BUILDFLAG(IS_ANDROID)
  void OnBrowserOpened(Browser* browser);
  void OnBrowserClosed(Browser* browser);

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

  void OnClosingAllBrowsersChanged(bool closing);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Destroy after |profile_attributes_storage_| since Profile destruction may
  // trigger some observers to unregister themselves.
  base::ObserverList<ProfileManagerObserver, /*check_empty=*/true> observers_;

  // Object to cache various information about profiles. Contains information
  // about every profile which has been created for this instance of Chrome,
  // if it has not been explicitly deleted. It must be destroyed after
  // |profiles_info_| because ~ProfileInfo can trigger a chain of events leading
  // to an access to this member.
  std::unique_ptr<ProfileAttributesStorage> profile_attributes_storage_;

#if BUILDFLAG(IS_ANDROID)
  // Handles the communication with the Java ProfileManager.
  std::unique_ptr<ProfileManagerAndroid> profile_manager_android_;
#endif  // BUILDFLAG(IS_ANDROID)

  base::CallbackListSubscription closing_all_browsers_subscription_;

  // The path to the user data directory (DIR_USER_DATA).
  const base::FilePath user_data_dir_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_ = false;

#if !BUILDFLAG(IS_ANDROID)
  BrowserListObserver browser_list_observer_{this};

  std::unique_ptr<DeleteProfileHelper> delete_profile_helper_;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Maps profile path to `ProfileInfo` (if profile has been loaded). Use
  // `RegisterProfile()` to add into this map. This map owns all loaded profile
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
  std::vector<raw_ptr<Profile, VectorExperimental>> active_profiles_;
  bool closing_all_browsers_ = false;

  // Tracks whether the the list of last opened Profiles has been updated for
  // the current session. If this is false `GetLastOpenedProfiles()` will return
  // the list of Profiles that were open the last time Chrome was running.
  bool has_updated_last_opened_profiles_ = false;

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
