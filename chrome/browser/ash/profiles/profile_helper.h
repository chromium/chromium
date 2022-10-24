// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "components/user_manager/user_manager.h"

class IndependentOTRProfileManagerTest;
class Profile;

namespace base {
class FilePath;
}

namespace ash {

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
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  ProfileHelper();

  ProfileHelper(const ProfileHelper&) = delete;
  ProfileHelper& operator=(const ProfileHelper&) = delete;

  ~ProfileHelper() override;

  // Creates and returns ProfileHelper implementation instance to
  // BrowserProcess/BrowserProcessPlatformPart.
  static std::unique_ptr<ProfileHelper> CreateInstance();

  // Returns ProfileHelper instance. This class is not singleton and is owned
  // by BrowserProcess/BrowserProcessPlatformPart. This method keeps that
  // knowledge in one place.
  static ProfileHelper* Get();

  // DEPRECATED: Please use
  // BrowserContextHelper::GetBrowserContextPathByUserIdHash() instead.
  // Returns profile path that corresponds to a given |user_id_hash|.
  static base::FilePath GetProfilePathByUserIdHash(
      const std::string& user_id_hash);

  // Returns the path that corresponds to the sign-in profile.
  static base::FilePath GetSigninProfileDir();

  // Returns OffTheRecord profile for use during signing phase.
  static Profile* GetSigninProfile();

  // DEPRECATED. Please use
  // ash::BrowserContextHelper::GetUserIdHashFromBrowserContext() instead.
  // Returns user_id hash for |profile| instance or empty string if hash
  // could not be extracted from |profile|.
  static std::string GetUserIdHashFromProfile(const Profile* profile);

  // DEPRECATED. Please use
  // ash::BrowserContextHelper::GetUserBrowserContextDirName() instead.
  // Returns user profile dir in a format [u-user_id_hash].
  static base::FilePath GetUserProfileDir(const std::string& user_id_hash);

  // Returns true if |profile| is the signin Profile. This can be used during
  // construction of the signin Profile to determine if that Profile is the
  // signin Profile.
  static bool IsSigninProfile(const Profile* profile);

  // Returns true if the signin profile has been initialized.
  static bool IsSigninProfileInitialized();

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

  // Returns the path that corresponds to the lockscreen profile.
  static base::FilePath GetLockScreenProfileDir();

  // Returns OffTheRecord profile for use during online authentication on the
  // lock screen.
  static Profile* GetLockScreenProfile();

  // Returns true if |profile| is the lockscreen profile.
  static bool IsLockScreenProfile(const Profile* profile);

  // Returns true when |profile| corresponds to owner's profile.
  static bool IsOwnerProfile(const Profile* profile);

  // Returns true when |profile| corresponds to the primary user profile
  // of the current session.
  static bool IsPrimaryProfile(const Profile* profile);

  // Returns true when |profile| is for an ephemeral user.
  static bool IsEphemeralUserProfile(const Profile* profile);

  // Returns true if profile or profile_path has corresponding chrome os user.
  // I.e. it is not one for internal use, such as sign-in or lockscreen etc.
  // Note: System and Guest Profiles are considered User profiles. To check on
  // that `Profile` specific method that checks the profile type should used
  // such as `Profile::IsRegularProfile()` or `Profile::IsSystemProfile()`.
  static bool IsUserProfile(const Profile* profile);
  static bool IsUserProfilePath(const base::FilePath& profile_path);

  // Returns active user profile dir in a format [u-$hash].
  virtual base::FilePath GetActiveUserProfileDir() = 0;

  // Should called once after UserManager instance has been created.
  virtual void Initialize() = 0;

  // Returns profile of the user associated with |account_id| if it is created
  // and fully initialized. Otherwise, returns NULL.
  virtual Profile* GetProfileByAccountId(const AccountId& account_id) = 0;

  // Returns profile of the |user| if it is created and fully initialized.
  // Otherwise, returns NULL.
  virtual Profile* GetProfileByUser(const user_manager::User* user) = 0;

  // Returns NULL if User is not created.
  virtual const user_manager::User* GetUserByProfile(
      const Profile* profile) const = 0;
  virtual user_manager::User* GetUserByProfile(Profile* profile) const = 0;

  // Enables/disables testing GetUserByProfile() by always returning
  // primary user.
  static void SetAlwaysReturnPrimaryUserForTesting(bool value);

  // Flushes all files of |profile|.
  virtual void FlushProfile(Profile* profile) = 0;

  // DEPRECATED: please set up UserManager.
  // Associates |user| with profile with the same user_id,
  // for GetUserByProfile() testing.
  virtual void SetProfileToUserMappingForTesting(user_manager::User* user) = 0;

  // DEPRECATED: please set up UserManager and create a Profile tied to a user
  // by its path. You may be interested in to create a testing profile by
  // TestingProfileManager.
  // Associates |profile| with |user|, for GetProfileByUser() testing.
  virtual void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                                 Profile* profile) = 0;

  // DEPRECATED: avoiding SetProfileToUserMappingForTesting will help
  // to remove this function's invocations.
  // Removes |account_id| user from |user_to_profile_for_testing_| for testing.
  virtual void RemoveUserFromListForTesting(const AccountId& account_id) = 0;

 protected:
  // TODO(nkostylev): Create a test API class that will be the only one allowed
  // to access private test methods.
  friend class FakeChromeUserManager;
  friend class MockUserManager;
  friend class ProfileHelperTest;
  friend class ::IndependentOTRProfileManagerTest;

  // Enables/disables testing code path in GetUserByProfile() like
  // always return primary user (when always_return_primary_user_for_testing is
  // set).
  static void SetProfileToUserForTestingEnabled(bool enabled);

  // If true testing code path is used in GetUserByProfile() even if
  // user_list_for_testing_ list is empty. In that case primary user will always
  // be returned.
  static bool enable_profile_to_user_testing;

  // If true and enable_profile_to_user_testing is true then primary user will
  // always be returned by GetUserByProfile().
  static bool always_return_primary_user_for_testing;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ProfileHelper;
}

#endif  // CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_
