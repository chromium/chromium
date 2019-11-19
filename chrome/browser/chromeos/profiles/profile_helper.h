// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browsing_data_remover.h"

class IndependentOTRProfileManagerTest;
class Profile;

namespace base {
class FilePath;
}

namespace chromeos {

class FileFlusher;

// This helper class is used on Chrome OS to keep track of currently
// active user profile.
// Whenever active user is changed (either add another user into session or
// switch between users), ActiveUserHashChanged() will be called thus
// internal state |active_user_id_hash_| will be updated.
// Typical use cases for using this class:
// 1. Get "signin profile" which is a special type of profile that is only used
//    during signin flow: GetSigninProfile()
// 2. Get profile dir of an active user, used by ProfileManager:
//    GetActiveUserProfileDir()
// 3. Get mapping from user_id_hash to Profile instance/profile path etc.
class ProfileHelper
    : public content::BrowsingDataRemover::Observer,
      public OAuth2LoginManager::Observer,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  ProfileHelper();
  ~ProfileHelper() override;

  // Returns ProfileHelper instance. This class is not singleton and is owned
  // by BrowserProcess/BrowserProcessPlatformPart. This method keeps that
  // knowledge in one place.
  static ProfileHelper* Get();

  // Loads and returns Profile instance that corresponds to |user_id_hash| for
  // test. It should not be used in production code because it could load a
  // not-yet-loaded user profile and skip the user profile initialization code
  // in UserSessionManager.
  // See http://crbug.com/728683 and http://crbug.com/718734.
  static Profile* GetProfileByUserIdHashForTest(
      const std::string& user_id_hash);

  // Returns profile path that corresponds to a given |user_id_hash|.
  static base::FilePath GetProfilePathByUserIdHash(
      const std::string& user_id_hash);

  // Returns the path that corresponds to the sign-in profile.
  static base::FilePath GetSigninProfileDir();

  // Returns OffTheRecord profile for use during signing phase.
  static Profile* GetSigninProfile();

  // Returns user_id hash for |profile| instance or empty string if hash
  // could not be extracted from |profile|.
  static std::string GetUserIdHashFromProfile(const Profile* profile);

  // Returns user profile dir in a format [u-user_id_hash].
  static base::FilePath GetUserProfileDir(const std::string& user_id_hash);

  // Returns true if |profile| is the signin Profile. This can be used during
  // construction of the signin Profile to determine if that Profile is the
  // signin Profile.
  static bool IsSigninProfile(const Profile* profile);

  // Returns true if the signin profile has been initialized.
  static bool IsSigninProfileInitialized();

  // Returns true if the signin profile has force-installed extensions set by
  // policy. This DCHECKs that the profile is created, its PrefService is
  // initialized and the associated pref exists.
  static bool SigninProfileHasLoginScreenExtensions();

  // Returns the path used for the lock screen apps profile - profile used
  // for launching platform apps that can display windows on top of the lock
  // screen.
  static base::FilePath GetLockScreenAppProfilePath();

  // Returns the name used for the lock screen app profile.
  static std::string GetLockScreenAppProfileName();

  // Returns whether |profile| is the lock screen app profile - the profile used
  // for launching platform apps that can display a window on top of the lock
  // screen.
  static bool IsLockScreenAppProfile(const Profile* profile);

  // Returns true when |profile| corresponds to owner's profile.
  static bool IsOwnerProfile(const Profile* profile);

  // Returns true when |profile| corresponds to the primary user profile
  // of the current session.
  static bool IsPrimaryProfile(const Profile* profile);

  // Returns true when |profile| is for an ephemeral user.
  static bool IsEphemeralUserProfile(const Profile* profile);

  // Return true if |profile| or |profile_path| corrrespond to a regular
  // (non-sign-in and non-lockscreen) profile.
  static bool IsRegularProfile(const Profile* profile);
  static bool IsRegularProfilePath(const base::FilePath& profile_path);

  // Initialize a bunch of services that are tied to a browser profile.
  // TODO(dzhioev): Investigate whether or not this method is needed.
  void ProfileStartup(Profile* profile);

  // Returns active user profile dir in a format [u-$hash].
  base::FilePath GetActiveUserProfileDir();

  // Should called once after UserManager instance has been created.
  void Initialize();

  // Clears site data (cookies, history, etc) for signin profile.
  // Callback can be empty. Not thread-safe.
  void ClearSigninProfile(const base::Closure& on_clear_callback);

  // Returns profile of the user associated with |account_id| if it is created
  // and fully initialized. Otherwise, returns NULL.
  Profile* GetProfileByAccountId(const AccountId& account_id);

  // Returns profile of the |user| if it is created and fully initialized.
  // Otherwise, returns NULL.
  Profile* GetProfileByUser(const user_manager::User* user);

  // DEPRECATED
  // Returns profile of the |user| if user's profile is created and fully
  // initialized. Otherwise, if some user is active, returns their profile.
  // Otherwise, returns signin profile.
  // Behaviour of this function does not correspond to its name and can be
  // very surprising, that's why it should not be used anymore.
  // Use |GetProfileByUser| instead.
  // TODO(dzhioev): remove this method. http://crbug.com/361528
  Profile* GetProfileByUserUnsafe(const user_manager::User* user);

  // Returns NULL if User is not created.
  const user_manager::User* GetUserByProfile(const Profile* profile) const;
  user_manager::User* GetUserByProfile(Profile* profile) const;

  static std::string GetUserIdHashByUserIdForTesting(
      const std::string& user_id);

  void SetActiveUserIdForTesting(const std::string& user_id);

  // Flushes all files of |profile|.
  void FlushProfile(Profile* profile);

  // Associates |user| with profile with the same user_id,
  // for GetUserByProfile() testing.
  void SetProfileToUserMappingForTesting(user_manager::User* user);

  // Enables/disables testing GetUserByProfile() by always returning
  // primary user.
  static void SetAlwaysReturnPrimaryUserForTesting(bool value);

  // Associates |profile| with |user|, for GetProfileByUser() testing.
  void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                         Profile* profile);

  // Removes |account_id| user from |user_to_profile_for_testing_| for testing.
  void RemoveUserFromListForTesting(const AccountId& account_id);

 private:
  // TODO(nkostylev): Create a test API class that will be the only one allowed
  // to access private test methods.
  friend class FakeChromeUserManager;
  friend class MockUserManager;
  friend class ProfileHelperTest;
  friend class ::IndependentOTRProfileManagerTest;

  // Called when signin profile is cleared.
  void OnSigninProfileCleared();

  // BrowsingDataRemover::Observer implementation:
  void OnBrowsingDataRemoverDone() override;

  // OAuth2LoginManager::Observer overrides.
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // user_manager::UserManager::UserSessionStateObserver implementation:
  void ActiveUserHashChanged(const std::string& hash) override;

  // Enables/disables testing code path in GetUserByProfile() like
  // always return primary user (when always_return_primary_user_for_testing is
  // set).
  static void SetProfileToUserForTestingEnabled(bool enabled);

  // Identifies path to active user profile on Chrome OS.
  std::string active_user_id_hash_;

  // List of callbacks called after signin profile clearance.
  std::vector<base::Closure> on_clear_callbacks_;

  // Called when a single stage of profile clearing is finished.
  base::Closure on_clear_profile_stage_finished_;

  // A currently running browsing data remover.
  content::BrowsingDataRemover* browsing_data_remover_;

  // Used for testing by unit tests and FakeUserManager/MockUserManager.
  std::map<const user_manager::User*, Profile*> user_to_profile_for_testing_;

  // When this list is not empty GetUserByProfile() will find user that has
  // the same user_id as |profile|->GetProfileName().
  user_manager::UserList user_list_for_testing_;

  // If true testing code path is used in GetUserByProfile() even if
  // user_list_for_testing_ list is empty. In that case primary user will always
  // be returned.
  static bool enable_profile_to_user_testing;

  // If true and enable_profile_to_user_testing is true then primary user will
  // always be returned by GetUserByProfile().
  static bool always_return_primary_user_for_testing;

  std::unique_ptr<FileFlusher> profile_flusher_;

  base::WeakPtrFactory<ProfileHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_
