// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

class AccountId;
class IndependentOTRProfileManagerTest;
class Profile;

namespace user_manager {
class User;
}

namespace ash {

// This helper class is used on Chrome OS to keep track of currently
// active user profile.
// Typical use cases for using this class:
// 1. Get "signin profile" which is a special type of profile that is only used
//    during signin flow: GetSigninProfile()
// 2. Get mapping from user_id_hash to Profile instance/profile path etc.
class ProfileHelper {
 public:
  ProfileHelper();

  ProfileHelper(const ProfileHelper&) = delete;
  ProfileHelper& operator=(const ProfileHelper&) = delete;

  virtual ~ProfileHelper();

  // Creates and returns ProfileHelper implementation instance to
  // BrowserProcess/BrowserProcessPlatformPart.
  static std::unique_ptr<ProfileHelper> CreateInstance();

  // Returns ProfileHelper instance. This class is not singleton and is owned
  // by BrowserProcess/BrowserProcessPlatformPart. This method keeps that
  // knowledge in one place.
  static ProfileHelper* Get();

  // DEPRECATED: Please use
  // ash::BrowserContextHelper::GetBrowserContextPathByUserIdHash() instead.
  // Returns profile path that corresponds to a given |user_id_hash|.
  static base::FilePath GetProfilePathByUserIdHash(
      const std::string& user_id_hash);

  // DEPRECATED: Please use
  // ash::BrowserContextHelper::GetSigninBrowserContext() instead.
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

  // DEPRECATED. Please use ash::IsSigninBrowserContext() instead.
  // Returns true if |profile| is the signin Profile. This can be used during
  // construction of the signin Profile to determine if that Profile is the
  // signin Profile.
  static bool IsSigninProfile(const Profile* profile);

  // DEPRECATED. Please use
  // ash::BrowserContextHelper::GetLockScreenAppBrowserContextPath() instead.
  // Returns the path used for the lock screen apps profile - profile used
  // for launching platform apps that can display windows on top of the lock
  // screen.
  static base::FilePath GetLockScreenAppProfilePath();

  // DEPRECATED. Please use ash::IsLockScreenAppBrowserContext() instead.
  // Returns whether |profile| is the lock screen app profile - the profile used
  // for launching platform apps that can display a window on top of the lock
  // screen.
  static bool IsLockScreenAppProfile(const Profile* profile);

  // DEPRECATED. Please use
  // ash::BrowserContextHelper::GetLockScreenBrowserContextPath() instead.
  // Returns the path that corresponds to the lockscreen profile.
  static base::FilePath GetLockScreenProfileDir();

  // DEPRECATED. Please use
  // ash::BrowserContextHelper::GetLockScreenBrowserContext() instead.
  // Returns OffTheRecord profile for use during online authentication on the
  // lock screen.
  static Profile* GetLockScreenProfile();

  // DEPRECATED. Please use ash::IsLockScreenBrowserContext() instead.
  // Returns true if |profile| is the lockscreen profile.
  static bool IsLockScreenProfile(const Profile* profile);

  // DEPRECATED. Please use
  // user_manager::UserManager::Get()->IsOwnerUser(
  //     BrowserContextHelper::Get()->GetUserByBrowserContext(profile))
  // instead.
  // Returns true when |profile| corresponds to owner's profile.
  static bool IsOwnerProfile(const Profile* profile);

  // DEPRECATED. Please use
  // user_manager::UserManager::Get()->IsPrimaryUser(
  //     BrowserContextHelper::Get()->GetUserByBrowserContext(profile))
  // instead.
  // Returns true when |profile| corresponds to the primary user profile
  // of the current session.
  static bool IsPrimaryProfile(const Profile* profile);

  // DEPRECATED. Please use
  // user_manager::UserManager::Get()->IsEphemeralUser(
  //     BrowserContextHelper::Get()->GetUserByBrowserContext(profile))
  // instead.
  // Returns true when |profile| is for an ephemeral user.
  static bool IsEphemeralUserProfile(const Profile* profile);

  // DEPRECATED. Please use ash::IsUserBrowserContext() instead.
  // Returns true if profile or profile_path has corresponding chrome os user.
  // I.e. it is not one for internal use, such as sign-in or lockscreen etc.
  // Note: System and Guest Profiles are considered User profiles. To check on
  // that `Profile` specific method that checks the profile type should used
  // such as `Profile::IsRegularProfile()` or `Profile::IsSystemProfile()`.
  static bool IsUserProfile(const Profile* profile);

  // DEPRECATED. Please use ash::IsUserBrowserContextBaseName() instead.
  static bool IsUserProfilePath(const base::FilePath& profile_path);

  // DEPRECATED: Please use
  // BrowserContextHelper::GetBrowserContextByAccountId() instead.
  // Returns profile of the user associated with |account_id| if it is created
  // and fully initialized. Otherwise, returns NULL.
  virtual Profile* GetProfileByAccountId(const AccountId& account_id) = 0;

  // DEPRECATED: Please use
  // BrowserContextHelper::GetBrowserContextByUser() instead.
  // Returns profile of the |user| if it is created and fully initialized.
  // Otherwise, returns NULL.
  virtual Profile* GetProfileByUser(const user_manager::User* user) = 0;

  // DEPRECATED: Please use
  // BrowserContextHelper::GetUserByBrowserContext() instead.
  // Returns NULL if User is not created.
  virtual const user_manager::User* GetUserByProfile(
      const Profile* profile) const = 0;
  virtual user_manager::User* GetUserByProfile(Profile* profile) const = 0;

  // Enables/disables testing GetUserByProfile() by always returning
  // primary user.
  static void SetAlwaysReturnPrimaryUserForTesting(bool value);

  // DEPRECATED: please set up UserManager and create a Profile tied to a user
  // by its path. You may be interested in to create a testing profile by
  // TestingProfileManager.
  // Associates |profile| with |user|, for GetProfileByUser() testing.
  virtual void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                                 Profile* profile) = 0;

  // Enables/disables testing code path in GetUserByProfile() like
  // always return primary user (when always_return_primary_user_for_testing is
  // set).
  static void SetProfileToUserForTestingEnabled(bool enabled);

 protected:
  // TODO(nkostylev): Create a test API class that will be the only one allowed
  // to access private test methods.
  friend class FakeChromeUserManager;
  friend class ProfileHelperTest;
  friend class ::IndependentOTRProfileManagerTest;

  // If true testing code path is used in GetUserByProfile() even if
  // user_list_for_testing_ list is empty. In that case primary user will always
  // be returned.
  static bool enable_profile_to_user_testing;

  // If true and enable_profile_to_user_testing is true then primary user will
  // always be returned by GetUserByProfile().
  static bool always_return_primary_user_for_testing;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_H_
