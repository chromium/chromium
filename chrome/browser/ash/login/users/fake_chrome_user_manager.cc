// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/users/avatar/mock_user_image_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner() = default;

  FakeTaskRunner(const FakeTaskRunner&) = delete;
  FakeTaskRunner& operator=(const FakeTaskRunner&) = delete;

 protected:
  ~FakeTaskRunner() override {}

 private:
  // base::SingleThreadTaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    std::move(task).Run();
    return true;
  }
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }
  bool RunsTasksInCurrentSequence() const override { return true; }
};

}  // namespace

namespace ash {

FakeChromeUserManager::FakeChromeUserManager()
    : ChromeUserManager(new FakeTaskRunner()) {
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
      account_id, false, user_manager::USER_TYPE_CHILD, nullptr);
}

user_manager::User* FakeChromeUserManager::AddUserWithAffiliation(
    const AccountId& account_id,
    bool is_affiliated) {
  return AddUserWithAffiliationAndTypeAndProfile(
      account_id, is_affiliated, user_manager::USER_TYPE_REGULAR, nullptr);
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
  user->SetAffiliation(is_affiliated);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  user->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);

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
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddArcKioskAppUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreateArcKioskAppUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddWebKioskAppUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreateWebKioskAppUser(account_id);
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
  users_.push_back(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddGuestUser() {
  user_manager::User* user =
      user_manager::User::CreateGuestUser(GetGuestAccountId());
  user->set_username_hash(
      user_manager::FakeUserManager::GetFakeUsernameHash(user->GetAccountId()));
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
      user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddActiveDirectoryUser(
    const AccountId& account_id) {
  DCHECK(account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY);
  return AddUserWithAffiliationAndTypeAndProfile(
      account_id, /*is_affiliated=*/false,
      user_manager::USER_TYPE_ACTIVE_DIRECTORY,
      /*profile=*/nullptr);
}

void FakeChromeUserManager::LoginUser(const AccountId& account_id,
                                      bool set_profile_created_flag) {
  UserLoggedIn(account_id,
               user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
               false /* browser_restart */, false /* is_child */);

  if (!set_profile_created_flag)
    return;

  // NOTE: This does not match production. See function comment.
  SimulateUserProfileLoad(account_id);
}

MultiProfileUserController*
FakeChromeUserManager::GetMultiProfileUserController() {
  return multi_profile_user_controller_;
}

UserImageManager* FakeChromeUserManager::GetUserImageManager(
    const AccountId& account_id) {
  UserImageManagerMap::iterator user_image_manager_it =
      user_image_managers_.find(account_id);
  if (user_image_manager_it != user_image_managers_.end())
    return user_image_manager_it->second.get();
  if (mock_user_image_manager_enabled_) {
    auto mgr =
        std::make_unique<::testing::NiceMock<MockUserImageManager>>(account_id);
    MockUserImageManager* mgr_raw = mgr.get();
    user_image_managers_[account_id] = std::move(mgr);
    return mgr_raw;
  }
  auto mgr = std::make_unique<UserImageManagerImpl>(account_id, this);
  UserImageManagerImpl* mgr_raw = mgr.get();
  user_image_managers_[account_id] = std::move(mgr);
  return mgr_raw;
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

void FakeChromeUserManager::RemoveUser(const AccountId& account_id,
                                       user_manager::UserRemovalReason reason) {
  // TODO(b/278643115): Unify the implementation with the real one.
  NotifyUserToBeRemoved(account_id);
  RemoveUserFromList(account_id);
  NotifyUserRemoved(account_id, reason);
}

void FakeChromeUserManager::RemoveUserFromList(const AccountId& account_id) {
  WallpaperControllerClientImpl* const wallpaper_client =
      WallpaperControllerClientImpl::Get();
  // `wallpaper_client` could be nullptr in tests.
  if (wallpaper_client)
    wallpaper_client->RemoveUserWallpaper(account_id, base::DoNothing());
  ProfileHelper::Get()->RemoveUserFromListForTesting(account_id);

  const user_manager::UserList::iterator it =
      base::ranges::find(users_, account_id, &user_manager::User::GetAccountId);
  if (it != users_.end()) {
    if (primary_user_ == *it)
      primary_user_ = nullptr;
    if (active_user_ != *it)
      delete *it;
    users_.erase(it);
  }
}

user_manager::UserList FakeChromeUserManager::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::User* user : users) {
    if (user->GetType() == user_manager::USER_TYPE_REGULAR &&
        !user->is_logged_in()) {
      result.push_back(user);
    }
  }

  return result;
}

void FakeChromeUserManager::SetOwnerId(const AccountId& account_id) {
  UserManagerBase::SetOwnerId(account_id);
}

const AccountId& FakeChromeUserManager::GetGuestAccountId() const {
  return user_manager::GuestAccountId();
}

void FakeChromeUserManager::AsyncRemoveCryptohome(
    const AccountId& account_id) const {
  NOTIMPLEMENTED();
}

bool FakeChromeUserManager::IsGuestAccountId(
    const AccountId& account_id) const {
  return account_id == user_manager::GuestAccountId();
}

bool FakeChromeUserManager::IsStubAccountId(const AccountId& account_id) const {
  return account_id == user_manager::StubAccountId();
}

bool FakeChromeUserManager::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         user_manager::kSupervisedUserDomain;
}

const gfx::ImageSkia& FakeChromeUserManager::GetResourceImagekiaNamed(
    int id) const {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

std::u16string FakeChromeUserManager::GetResourceStringUTF16(
    int string_id) const {
  return std::u16string();
}

void FakeChromeUserManager::ScheduleResolveLocale(
    const std::string& locale,
    base::OnceClosure on_resolved_callback,
    std::string* out_resolved_locale) const {
  NOTIMPLEMENTED();
  return;
}

bool FakeChromeUserManager::IsValidDefaultUserImageId(int image_index) const {
  return default_user_image::IsValidIndex(image_index);
}

// UserManager implementation:
void FakeChromeUserManager::Initialize() {
  return ChromeUserManager::Initialize();
}

void FakeChromeUserManager::Shutdown() {
  ChromeUserManager::Shutdown();

  for (auto& user_image_manager : user_image_managers_) {
    user_image_manager.second->Shutdown();
  }
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
  return logged_in_users_;
}

const AccountId& FakeChromeUserManager::GetLastSessionActiveAccountId() const {
  return last_session_active_account_id_;
}

void FakeChromeUserManager::UserLoggedIn(const AccountId& account_id,
                                         const std::string& username_hash,
                                         bool browser_restart,
                                         bool is_child) {
  for (auto* user : users_) {
    if (user->username_hash() == username_hash) {
      user->set_is_logged_in(true);
      logged_in_users_.push_back(user);

      if (!primary_user_)
        primary_user_ = user;
      break;
    }
  }
  // TODO(jamescook): This should set active_user_ and call NotifyOnLogin().
}

void FakeChromeUserManager::SwitchToLastActiveUser() {
  NOTREACHED();
}

bool FakeChromeUserManager::IsKnownUser(const AccountId& account_id) const {
  return true;
}

const user_manager::User* FakeChromeUserManager::FindUser(
    const AccountId& account_id) const {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id)
    return active_user_;

  const user_manager::UserList& users = GetUsers();
  for (const auto* user : users) {
    if (user->GetAccountId() == account_id)
      return user;
  }

  return nullptr;
}

user_manager::User* FakeChromeUserManager::FindUserAndModify(
    const AccountId& account_id) {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id)
    return active_user_;

  const user_manager::UserList& users = GetUsers();
  for (auto* user : users) {
    if (user->GetAccountId() == account_id)
      return user;
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
  NOTREACHED();
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
  for (auto* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_display_name(display_name);
      return;
    }
  }
}

std::u16string FakeChromeUserManager::GetUserDisplayName(
    const AccountId& account_id) const {
  return std::u16string();
}

void FakeChromeUserManager::SaveUserDisplayEmail(
    const AccountId& account_id,
    const std::string& display_email) {
  NOTREACHED();
}

void FakeChromeUserManager::SaveUserType(const user_manager::User* user) {
  NOTREACHED();
}

absl::optional<std::string> FakeChromeUserManager::GetOwnerEmail() {
  return GetLocalState() ? UserManagerBase::GetOwnerEmail() : absl::nullopt;
}

bool FakeChromeUserManager::IsCurrentUserOwner() const {
  return active_user_ && GetOwnerAccountId() == active_user_->GetAccountId();
}

bool FakeChromeUserManager::IsCurrentUserNonCryptohomeDataEphemeral() const {
  return current_user_ephemeral_;
}

bool FakeChromeUserManager::IsCurrentUserCryptohomeDataEphemeral() const {
  return current_user_ephemeral_;
}

bool FakeChromeUserManager::CanCurrentUserLock() const {
  return current_user_can_lock_;
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

bool FakeChromeUserManager::IsLoggedInAsPublicAccount() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsGuest() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == user_manager::USER_TYPE_GUEST
                     : false;
}

bool FakeChromeUserManager::IsLoggedInAsKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::USER_TYPE_KIOSK_APP
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsArcKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP
             : false;
}

bool FakeChromeUserManager::IsLoggedInAsWebKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::USER_TYPE_WEB_KIOSK_APP
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
  DCHECK(user.GetType() == user_manager::USER_TYPE_REGULAR ||
         user.GetType() == user_manager::USER_TYPE_GUEST ||
         user.GetType() == user_manager::USER_TYPE_CHILD);

  if (user.GetType() == user_manager::USER_TYPE_GUEST &&
      !IsGuestSessionAllowed())
    return false;
  if (user.HasGaiaAccount() && !IsGaiaUserAllowed(user))
    return false;
  return true;
}

void FakeChromeUserManager::SimulateUserProfileLoad(
    const AccountId& account_id) {
  for (auto* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->SetProfileIsCreated();
      break;
    }
  }
}

const std::string& FakeChromeUserManager::GetApplicationLocale() const {
  static const std::string default_locale("en-US");
  return default_locale;
}

void FakeChromeUserManager::LoadDeviceLocalAccounts(
    std::set<AccountId>* users_set) {
  NOTREACHED();
}

bool FakeChromeUserManager::IsEnterpriseManaged() const {
  return is_enterprise_managed_;
}

void FakeChromeUserManager::PerformPostUserLoggedInActions(
    bool browser_restart) {
  NOTREACHED();
}

bool FakeChromeUserManager::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return false;
}

void FakeChromeUserManager::KioskAppLoggedIn(user_manager::User* user) {}

void FakeChromeUserManager::PublicAccountUserLoggedIn(
    user_manager::User* user) {
  NOTREACHED();
}

void FakeChromeUserManager::SetUserAffiliation(
    const AccountId& account_id,
    const base::flat_set<std::string>& user_affiliation_ids) {}

void FakeChromeUserManager::SetUserAffiliationForTesting(
    const AccountId& account_id,
    bool is_affiliated) {
  auto* user = FindUserAndModify(account_id);
  if (!user) {
    return;
  }
  user->SetAffiliation(is_affiliated);
  NotifyUserAffiliationUpdated(*user);
}

bool FakeChromeUserManager::IsEphemeralAccountIdByPolicy(
    const AccountId& account_id) const {
  return fake_ephemeral_mode_config_.IsAccountIdIncluded(account_id);
}

user_manager::User* FakeChromeUserManager::GetActiveUserInternal() const {
  if (active_user_ != nullptr)
    return active_user_;

  if (users_.empty())
    return nullptr;
  if (active_account_id_.is_valid()) {
    for (auto* user : users_) {
      if (user->GetAccountId() == active_account_id_)
        return user;
    }
  }
  return users_[0];
}

}  // namespace ash
