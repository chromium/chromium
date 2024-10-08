// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

FakeChromeUserManager::FakeChromeUserManager()
    : UserManagerImpl(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          g_browser_process ? g_browser_process->local_state() : nullptr,
          ash::CrosSettings::IsInitialized() ? ash::CrosSettings::Get()
                                             : nullptr) {
  ProfileHelper::SetProfileToUserForTestingEnabled(true);
}

FakeChromeUserManager::~FakeChromeUserManager() {
  ProfileHelper::SetProfileToUserForTestingEnabled(false);
}

user_manager::User* FakeChromeUserManager::AddUser(
    const AccountId& account_id) {
  return AddUserWithAffiliation(account_id, false);
}

user_manager::User* FakeChromeUserManager::AddChildUser(
    const AccountId& account_id) {
  return AddUserWithAffiliationAndTypeAndProfile(
      account_id, false, user_manager::UserType::kChild, nullptr);
}

user_manager::User* FakeChromeUserManager::AddUserWithAffiliation(
    const AccountId& account_id,
    bool is_affiliated) {
  return AddUserWithAffiliationAndTypeAndProfile(
      account_id, is_affiliated, user_manager::UserType::kRegular, nullptr);
}

user_manager::User* FakeChromeUserManager::AddSamlUser(
    const AccountId& account_id) {
  user_manager::User* user = AddUser(account_id);
  user->set_using_saml(true);
  return user;
}

user_manager::User*
FakeChromeUserManager::AddUserWithAffiliationAndTypeAndProfile(
    const AccountId& account_id,
    bool is_affiliated,
    user_manager::UserType user_type,
    TestingProfile* profile) {
  user_manager::User* user =
      user_manager::User::CreateRegularUser(account_id, user_type);
  user->SetAffiliated(is_affiliated);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::UserImage::Type::kProfile, false);
  user_storage_.emplace_back(user);
  users_.push_back(user);

  if (profile) {
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
  }

  NotifyUserAffiliationUpdated(*user);

  return user;
}

user_manager::User* FakeChromeUserManager::AddKioskAppUser(
    const AccountId& account_id) {
  user_manager::User* user = user_manager::User::CreateKioskAppUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddWebKioskAppUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreateWebKioskAppUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddKioskIwaUser(
    const AccountId& account_id) {
  user_manager::User* user = user_manager::User::CreateKioskIwaUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddGuestUser() {
  user_manager::User* user =
      user_manager::User::CreateGuestUser(user_manager::GuestAccountId());
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(user->GetAccountId()));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddPublicAccountUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreatePublicAccountUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::UserImage::Type::kProfile, false);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

void FakeChromeUserManager::LoginUser(const AccountId& account_id,
                                      bool set_profile_created_flag) {
  UserLoggedIn(account_id,
               user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
               false /* browser_restart */, false /* is_child */);

  if (!set_profile_created_flag) {
    return;
  }

  // NOTE: This does not match production. See function comment.
  SimulateUserProfileLoad(account_id);
}

void FakeChromeUserManager::SwitchActiveUser(const AccountId& account_id) {
  active_account_id_ = account_id;
  active_user_ = nullptr;
  if (!users_.empty() && active_account_id_.is_valid()) {
    for (user_manager::User* const user : users_) {
      if (user->GetAccountId() == active_account_id_) {
        active_user_ = user;
        user->set_is_active(true);
      } else {
        user->set_is_active(false);
      }
    }
  }
  NotifyActiveUserChanged(active_user_);
}

void FakeChromeUserManager::OnSessionStarted() {}

user_manager::UserList FakeChromeUserManager::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::UserType::kRegular) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::User* user : users) {
    if (user->GetType() == user_manager::UserType::kRegular &&
        !user->is_logged_in()) {
      result.push_back(user);
    }
  }

  return result;
}

bool FakeChromeUserManager::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         user_manager::kSupervisedUserDomain;
}

const user_manager::UserList& FakeChromeUserManager::GetUsers() const {
  return users_;
}

const user_manager::UserList& FakeChromeUserManager::GetLoggedInUsers() const {
  return logged_in_users_;
}

const user_manager::UserList& FakeChromeUserManager::GetLRULoggedInUsers()
    const {
  return logged_in_users_;
}

user_manager::UserList FakeChromeUserManager::GetUnlockUsers() const {
  // Test case UserPrefsChange expects that the list of the unlock users
  // depends on prefs::kAllowScreenLock.
  user_manager::UserList unlock_users;
  for (user_manager::User* user : logged_in_users_) {
    // Skip if user has a profile and kAllowScreenLock is set to false.
    if (user->GetProfilePrefs() &&
        !user->GetProfilePrefs()->GetBoolean(ash::prefs::kAllowScreenLock)) {
      continue;
    }
    unlock_users.push_back(user);
  }

  return unlock_users;
}

const AccountId& FakeChromeUserManager::GetLastSessionActiveAccountId() const {
  return last_session_active_account_id_;
}

void FakeChromeUserManager::UserLoggedIn(const AccountId& account_id,
                                         const std::string& username_hash,
                                         bool browser_restart,
                                         bool is_child) {
  // Please keep the implementation in sync with FakeUserManager::UserLoggedIn.
  // We're in process to merge.
  for (user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_is_logged_in(true);
      user->set_username_hash(username_hash);
      logged_in_users_.push_back(user);
      if (!primary_user_) {
        primary_user_ = user;
      }
      if (active_user_) {
        NotifyUserAddedToSession(user);
      } else {
        active_user_ = user;
      }
      break;
    }
  }

  if (!active_user_ && IsEphemeralAccountId(account_id)) {
    RegularUserLoggedInAsEphemeral(account_id,
                                   user_manager::UserType::kRegular);
  }

  NotifyOnLogin();
}

void FakeChromeUserManager::SwitchToLastActiveUser() {
  NOTREACHED_IN_MIGRATION();
}

bool FakeChromeUserManager::IsKnownUser(const AccountId& account_id) const {
  return true;
}

const user_manager::User* FakeChromeUserManager::FindUser(
    const AccountId& account_id) const {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id) {
    return active_user_;
  }

  const user_manager::UserList& users = GetUsers();
  for (const user_manager::User* user : users) {
    if (user->GetAccountId() == account_id) {
      return user;
    }
  }

  return nullptr;
}

user_manager::User* FakeChromeUserManager::FindUserAndModify(
    const AccountId& account_id) {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id) {
    return active_user_;
  }

  const user_manager::UserList& users = GetUsers();
  for (user_manager::User* user : users) {
    if (user->GetAccountId() == account_id) {
      return user;
    }
  }

  return nullptr;
}

const user_manager::User* FakeChromeUserManager::GetActiveUser() const {
  return GetActiveUserInternal();
}

user_manager::User* FakeChromeUserManager::GetActiveUser() {
  return GetActiveUserInternal();
}

const user_manager::User* FakeChromeUserManager::GetPrimaryUser() const {
  return primary_user_;
}

void FakeChromeUserManager::SaveUserOAuthStatus(
    const AccountId& account_id,
    user_manager::User::OAuthTokenStatus oauth_token_status) {
  NOTREACHED_IN_MIGRATION();
}

void FakeChromeUserManager::SaveForceOnlineSignin(const AccountId& account_id,
                                                  bool force_online_signin) {
  if (!active_user_ || active_user_->GetAccountId() != account_id) {
    // On the login screen we can update force_online_signin flag for
    // an arbitrary user.
    user_manager::User* const user = FindUserAndModify(account_id);
    if (user) {
      user->set_force_online_signin(force_online_signin);
    }
  } else {
    active_user_->set_force_online_signin(force_online_signin);
  }
}

void FakeChromeUserManager::SaveUserDisplayName(
    const AccountId& account_id,
    const std::u16string& display_name) {
  for (user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_display_name(display_name);
      return;
    }
  }
}

void FakeChromeUserManager::SaveUserDisplayEmail(
    const AccountId& account_id,
    const std::string& display_email) {
  user_manager::User* user = FindUserAndModify(account_id);
  if (!user) {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
    return;
  }
  user->set_display_email(display_email);
}

void FakeChromeUserManager::SaveUserType(const user_manager::User* user) {
  NOTREACHED_IN_MIGRATION();
}

std::optional<std::string> FakeChromeUserManager::GetOwnerEmail() {
  return GetLocalState() ? UserManagerImpl::GetOwnerEmail() : std::nullopt;
}

bool FakeChromeUserManager::IsCurrentUserNonCryptohomeDataEphemeral() const {
  return current_user_ephemeral_;
}

bool FakeChromeUserManager::IsCurrentUserCryptohomeDataEphemeral() const {
  return current_user_ephemeral_;
}

bool FakeChromeUserManager::IsUserLoggedIn() const {
  return logged_in_users_.size() > 0;
}

bool FakeChromeUserManager::IsLoggedInAsUserWithGaiaAccount() const {
  return true;
}

bool FakeChromeUserManager::IsLoggedInAsChildUser() const {
  return current_user_child_;
}

bool FakeChromeUserManager::IsLoggedInAsManagedGuestSession() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::UserType::kPublicAccount
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsGuest() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == user_manager::UserType::kGuest
                     : false;
}

bool FakeChromeUserManager::IsLoggedInAsKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::UserType::kKioskApp
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsWebKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::UserType::kWebKioskApp
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsAnyKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user && active_user->IsKioskType();
}

bool FakeChromeUserManager::IsLoggedInAsStub() const {
  return false;
}

bool FakeChromeUserManager::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  return current_user_ephemeral_;
}

bool FakeChromeUserManager::IsGuestSessionAllowed() const {
  bool is_guest_allowed = false;
  CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &is_guest_allowed);
  return is_guest_allowed;
}

bool FakeChromeUserManager::IsGaiaUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.HasGaiaAccount());
  return CrosSettings::Get()->IsUserAllowlisted(
      user.GetAccountId().GetUserEmail(), nullptr, user.GetType());
}

bool FakeChromeUserManager::IsUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.GetType() == user_manager::UserType::kRegular ||
         user.GetType() == user_manager::UserType::kGuest ||
         user.GetType() == user_manager::UserType::kChild);

  if (user.GetType() == user_manager::UserType::kGuest &&
      !IsGuestSessionAllowed()) {
    return false;
  }
  if (user.HasGaiaAccount() && !IsGaiaUserAllowed(user)) {
    return false;
  }
  return true;
}

void FakeChromeUserManager::SimulateUserProfileLoad(
    const AccountId& account_id) {
  for (user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->SetProfileIsCreated();
      break;
    }
  }
}

bool FakeChromeUserManager::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return false;
}

void FakeChromeUserManager::SetUserAffiliated(const AccountId& account_id,
                                              bool is_affiliated) {}

void FakeChromeUserManager::SetUserAffiliationForTesting(
    const AccountId& account_id,
    bool is_affiliated) {
  auto* user = FindUserAndModify(account_id);
  if (!user) {
    return;
  }
  user->SetAffiliated(is_affiliated);
  NotifyUserAffiliationUpdated(*user);
}

user_manager::User* FakeChromeUserManager::GetActiveUserInternal() const {
  if (active_user_ != nullptr) {
    return active_user_;
  }

  if (users_.empty()) {
    return nullptr;
  }
  if (active_account_id_.is_valid()) {
    for (user_manager::User* user : users_) {
      if (user->GetAccountId() == active_account_id_) {
        return user;
      }
    }
  }
  return users_[0];
}

}  // namespace ash
