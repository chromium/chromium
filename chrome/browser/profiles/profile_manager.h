// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ProfileAttributesStorage;
class ProfileInfoCache;
class ProfileManagerObserver;

class ProfileManager : public content::NotificationObserver,
                       public Profile::Delegate {
 public:
  using CreateCallback =
      base::RepeatingCallback<void(Profile*, Profile::CreateStatus)>;
  using ProfileLoadedCallback = base::OnceCallback<void(Profile*)>;

  explicit ProfileManager(const base::FilePath& user_data_dir);
  ~ProfileManager() override;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Invokes SessionServiceFactory::ShutdownForProfile() for all profiles.
  static void ShutdownSessionServices();
#endif

  // Physically remove deleted profile directories from disk.
  static void NukeDeletedProfilesFromDisk();

  // Same as instance method but provides the default user_data_dir as well.
  // If the Profile is going to be used to open a new window then consider using
  // GetLastUsedProfileAllowedByPolicy() instead.
  static Profile* GetLastUsedProfile();

  // Same as GetLastUsedProfile() but returns the incognito Profile if
  // incognito mode is forced. This should be used if the last used Profile
  // will be used to open new browser windows.
  static Profile* GetLastUsedProfileAllowedByPolicy();

  // Helper function that returns true if incognito mode is forced for |profile|
  // (normal mode is not available for browsing).
  static bool IncognitoModeForced(Profile* profile);

  // Same as instance method but provides the default user_data_dir as well.
  static std::vector<Profile*> GetLastOpenedProfiles();

  // Get the profile for the user which created the current session.
  // Note that in case of a guest account this will return a 'suitable' profile.
  // This function is temporary and will soon be moved to ash. As such avoid
  // using it at all cost.
  // TODO(skuhne): Move into ash's new user management function.
  static Profile* GetPrimaryUserProfile();

  // Get the profile for the currently active user.
  // Note that in case of a guest account this will return a 'suitable' profile.
  // This function is temporary and will soon be moved to ash. As such avoid
  // using it at all cost.
  // TODO(skuhne): Move into ash's new user management function.
  static Profile* GetActiveUserProfile();

  // Load and return the initial profile for browser. On ChromeOS, this returns
  // either the sign-in profile or the active user profile depending on whether
  // browser is started normally or is restarted after crash. On other
  // platforms, this returns the default profile.
  static Profile* CreateInitialProfile();

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

  // Returns total number of profiles available on this machine.
  size_t GetNumberOfProfiles();

  // Asynchronously loads an existing profile given its |profile_name| within
  // the user data directory, optionally in |incognito| mode. The |callback|
  // will be called with the Profile when it has been loaded, or with a nullptr
  // otherwise. Should be called on the UI thread.
  // Unlike CreateProfileAsync this will not create a profile if one doesn't
  // already exist on disk
  // Returns true if the profile exists, but the final loaded profile will come
  // as part of the callback.
  bool LoadProfile(const std::string& profile_name,
                   bool incognito,
                   ProfileLoadedCallback callback);
  bool LoadProfileByPath(const base::FilePath& profile_path,
                         bool incognito,
                         ProfileLoadedCallback callback);

  // Explicit asynchronous creation of a profile located at |profile_path|.
  // If the profile has already been created then callback is called
  // immediately. Should be called on the UI thread.
  void CreateProfileAsync(const base::FilePath& profile_path,
                          const CreateCallback& callback,
                          const base::string16& name,
                          const std::string& icon_url);

  // Returns true if the profile pointer is known to point to an existing
  // profile.
  bool IsValidProfile(const void* profile);

  // Returns the directory where the first created profile is stored,
  // relative to the user data directory currently in use.
  base::FilePath GetInitialProfileDir();

  // Get the Profile last used (the Profile to which owns the most recently
  // focused window) with this Chrome build. If no signed profile has been
  // stored in Local State, hand back the Default profile.
  Profile* GetLastUsedProfile(const base::FilePath& user_data_dir);

  // Get the path of the last used profile, or if that's undefined, the default
  // profile.
  base::FilePath GetLastUsedProfileDir(const base::FilePath& user_data_dir);

  // Get the name of the last used profile, or if that's undefined, the default
  // profile.
  std::string GetLastUsedProfileName();

  // Get the Profiles which are currently open, i.e. have open browsers or were
  // open the last time Chrome was running. Profiles that fail to initialize are
  // skipped. The Profiles appear in the order they were opened. The last used
  // profile will be on the list if it is initialized successfully, but its
  // index on the list will depend on when it was opened (it is not necessarily
  // the last one).
  std::vector<Profile*> GetLastOpenedProfiles(
      const base::FilePath& user_data_dir);

  // Returns created and fully initialized profiles. Note, profiles order is NOT
  // guaranteed to be related with the creation order.
  std::vector<Profile*> GetLoadedProfiles() const;

  // If a profile with the given path is currently managed by this object and
  // fully initialized, return a pointer to the corresponding Profile object;
  // otherwise return null.
  Profile* GetProfileByPath(const base::FilePath& path) const;

  // Creates a new profile in the next available multiprofile directory.
  // Directories are named "profile_1", "profile_2", etc., in sequence of
  // creation. (Because directories can be removed, however, it may be the case
  // that at some point the list of numbered profiles is not continuous.)
  // |callback| may be invoked multiple times (for CREATE_STATUS_INITIALIZED
  // and CREATE_STATUS_CREATED) so binding parameters with bind::Passed() is
  // prohibited. Returns the file path to the profile that will be created
  // asynchronously.
  static base::FilePath CreateMultiProfileAsync(const base::string16& name,
                                                const std::string& icon_url,
                                                const CreateCallback& callback);

  // Returns the full path to be used for guest profiles.
  static base::FilePath GetGuestProfilePath();

  // Returns the full path to be used for system profiles.
  static base::FilePath GetSystemProfilePath();

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

#if !defined(OS_ANDROID)
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
#endif

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
  // If |start_deferred_task_runners|, starts the deferred task runners.
  // Use ONLY in tests.
  void RegisterTestingProfile(std::unique_ptr<Profile> profile,
                              bool addToStorage,
                              bool start_deferred_task_runners);

  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Profile::Delegate implementation:
  void OnProfileCreated(Profile* profile,
                        bool success,
                        bool is_new_profile) override;

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
      const base::FilePath& path,
      Delegate* delegate);

  void set_do_final_services_init(bool do_final_services_init) {
    do_final_services_init_ = do_final_services_init;
  }

 private:
  friend class TestingProfileManager;
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, DeleteAllProfiles);
  FRIEND_TEST_ALL_PREFIXES(ProfileManagerBrowserTest, SwitchToProfile);

  // This struct contains information about profiles which are being loaded or
  // were loaded.
  struct ProfileInfo {
    ProfileInfo(std::unique_ptr<Profile> profile, bool created);

    ~ProfileInfo();

    std::unique_ptr<Profile> profile;
    // Whether profile has been fully loaded (created and initialized).
    bool created;
    // List of callbacks to run when profile initialization is done. Note, when
    // profile is fully loaded this vector will be empty.
    std::vector<CreateCallback> callbacks;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProfileInfo);
  };

  // Does final initial actions.
  void DoFinalInit(ProfileInfo* profile_info, bool go_off_the_record);
  void DoFinalInitForServices(Profile* profile, bool go_off_the_record);
  void DoFinalInitLogging(Profile* profile);

  // Returns the profile of the active user and / or the off the record profile
  // if needed. This adds the profile to the ProfileManager if it doesn't
  // already exist. The method will return NULL if the profile doesn't exist
  // and we can't create it.
  // The profile used can be overridden by using --login-profile on cros.
  Profile* GetActiveUserOrOffTheRecordProfileFromPath(
      const base::FilePath& user_data_dir);

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(std::unique_ptr<Profile> profile);

  // Synchronously creates and returns a profile. This handles both the full
  // creation and adds it to the set managed by this ProfileManager. Returns
  // null if creation fails.
  Profile* CreateAndInitializeProfile(const base::FilePath& profile_dir);

#if !defined(OS_ANDROID)
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
#endif

  // Registers profile with given info. Returns pointer to created ProfileInfo
  // entry.
  ProfileInfo* RegisterProfile(std::unique_ptr<Profile> profile, bool created);

  // Returns ProfileInfo associated with given |path|, registered earlier with
  // RegisterProfile.
  ProfileInfo* GetProfileInfoByPath(const base::FilePath& path) const;

  // Returns a registered profile. In contrast to GetProfileByPath(), this will
  // also return a profile that is not fully initialized yet, so this method
  // should be used carefully.
  Profile* GetProfileByPathInternal(const base::FilePath& path) const;

  // Returns a ProfileInfoCache object which can be used to get information
  // about profiles without having to load them from disk.
  // Deprecated, use GetProfileAttributesStorage() instead.
  ProfileInfoCache& GetProfileInfoCache();

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

#if !defined(OS_ANDROID)
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
    ~BrowserListObserver() override;

    // ::BrowserListObserver implementation.
    void OnBrowserAdded(Browser* browser) override;
    void OnBrowserRemoved(Browser* browser) override;
    void OnBrowserSetLastActive(Browser* browser) override;

   private:
    ProfileManager* profile_manager_;
    DISALLOW_COPY_AND_ASSIGN(BrowserListObserver);
  };

  // If the |loaded_profile| has been loaded successfully (according to
  // |status|) and isn't already scheduled for deletion, then finishes adding
  // |profile_to_delete_dir| to the queue of profiles to be deleted, and updates
  // the kProfileLastUsed preference based on
  // |last_non_supervised_profile_path|.
  void OnNewActiveProfileLoaded(
      const base::FilePath& profile_to_delete_path,
      const base::FilePath& last_non_supervised_profile_path,
      ProfileLoadedCallback callback,
      Profile* loaded_profile,
      Profile::CreateStatus status);

  // Schedules the forced ephemeral profile at the given path to be deleted on
  // shutdown. New profiles will not be created.
  void ScheduleForcedEphemeralProfileForDeletion(
      const base::FilePath& profile_dir);
#endif  // !defined(OS_ANDROID)

  // Destroy after |profile_info_cache_| since Profile destruction may trigger
  // some observers to unregister themselves.
  base::ObserverList<ProfileManagerObserver> observers_;

  // Object to cache various information about profiles. Contains information
  // about every profile which has been created for this instance of Chrome,
  // if it has not been explicitly deleted. It must be destroyed after
  // |profiles_info_| because ~ProfileInfo can trigger a chain of events leading
  // to an access to this member.
  std::unique_ptr<ProfileInfoCache> profile_info_cache_;

  content::NotificationRegistrar registrar_;

  // The path to the user data directory (DIR_USER_DATA).
  const base::FilePath user_data_dir_;

  // Indicates that a user has logged in and that the profile specified
  // in the --login-profile command line argument should be used as the
  // default.
  bool logged_in_ = false;

#if !defined(OS_ANDROID)
  BrowserListObserver browser_list_observer_{this};
#endif  // !defined(OS_ANDROID)

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

  // Controls whether to initialize some services. Only disabled for testing.
  bool do_final_services_init_ = true;

  // TODO(chrome/browser/profiles/OWNERS): Usage of this in profile_manager.cc
  // should likely be turned into DCHECK_CURRENTLY_ON(BrowserThread::UI) for
  // consistency with surrounding code in the same file but that wasn't trivial
  // enough to do as part of the mass refactor CL which introduced
  // |thread_checker_|, ref. https://codereview.chromium.org/2907253003/#msg37.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ProfileManager);
};

// Same as the ProfileManager, but doesn't initialize some services of the
// profile. This one is useful in unittests.
class ProfileManagerWithoutInit : public ProfileManager {
 public:
  explicit ProfileManagerWithoutInit(const base::FilePath& user_data_dir);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileManagerWithoutInit);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_H_
