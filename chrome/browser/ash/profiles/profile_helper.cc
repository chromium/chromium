// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/profile_helper.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/base/file_flusher.h"
#include "chrome/browser/ash/profiles/browser_context_helper_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_types_ash.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace {

class UsernameHashMatcher {
 public:
  explicit UsernameHashMatcher(const std::string& h) : username_hash(h) {}
  bool operator()(const user_manager::User* user) const {
    return user->username_hash() == username_hash;
  }

 private:
  const std::string& username_hash;
};

}  // anonymous namespace

// static
bool ProfileHelper::enable_profile_to_user_testing = false;
bool ProfileHelper::always_return_primary_user_for_testing = false;

class ProfileHelperImpl : public ProfileHelper {
 public:
  explicit ProfileHelperImpl(
      std::unique_ptr<BrowserContextHelper::Delegate> delegate);
  ~ProfileHelperImpl() override;

  // Returns the path that corresponds to the passed profile.
  base::FilePath GetProfileDir(base::StringPiece profile);

  // Returns true if the signin profile has been initialized.
  bool IsSigninProfileInitialized();

  // Returns OffTheRecord profile for use during signing phase.
  Profile* GetSigninProfile();

  // Returns OffTheRecord profile for use during online authentication on the
  // lock screen.
  Profile* GetLockScreenProfile();

  // ProfileHelper overrides
  base::FilePath GetActiveUserProfileDir() override;
  void Initialize() override;

  Profile* GetProfileByAccountId(const AccountId& account_id) override;
  Profile* GetProfileByUser(const user_manager::User* user) override;

  const user_manager::User* GetUserByProfile(
      const Profile* profile) const override;
  user_manager::User* GetUserByProfile(Profile* profile) const override;

  void FlushProfile(Profile* profile) override;

  void SetProfileToUserMappingForTesting(user_manager::User* user) override;
  void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                         Profile* profile) override;
  void RemoveUserFromListForTesting(const AccountId& account_id) override;

 private:
  // user_manager::UserManager::UserSessionStateObserver implementation:
  void ActiveUserHashChanged(const std::string& hash) override;

  std::unique_ptr<BrowserContextHelper> browser_context_helper_;

  // Identifies path to active user profile on Chrome OS.
  std::string active_user_id_hash_;

  // Used for testing by unit tests and FakeUserManager/MockUserManager.
  std::map<const user_manager::User*, Profile*> user_to_profile_for_testing_;

  // When this list is not empty GetUserByProfile() will find user that has
  // the same user_id as |profile|->GetProfileName().
  user_manager::UserList user_list_for_testing_;

  std::unique_ptr<FileFlusher> profile_flusher_;
};

namespace {
// Convenient utility to obtain ProfileHelperImpl.
// Currently ProfileHelper interface is implemented by only ProfileHelperImpl,
// so safe to cast.
// TODO(crbug.com/1325210): Remove this after ProfileHelper is moved out from
// chrome/browser.
ProfileHelperImpl* GetImpl() {
  return static_cast<ProfileHelperImpl*>(ProfileHelper::Get());
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, public

ProfileHelper::ProfileHelper() {}

ProfileHelper::~ProfileHelper() {
  // Checking whether UserManager is initialized covers case
  // when ScopedTestUserManager is used.
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

// static
std::unique_ptr<ProfileHelper> ProfileHelper::CreateInstance() {
  return std::make_unique<ProfileHelperImpl>(
      std::make_unique<BrowserContextHelperDelegateImpl>());
}

// static
ProfileHelper* ProfileHelper::Get() {
  return g_browser_process->platform_part()->profile_helper();
}

// static
base::FilePath ProfileHelper::GetProfilePathByUserIdHash(
    const std::string& user_id_hash) {
  return BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
      user_id_hash);
}

// static
base::FilePath ProfileHelper::GetSigninProfileDir() {
  return GetImpl()->GetProfileDir(chrome::kInitialProfile);
}

// static
Profile* ProfileHelper::GetSigninProfile() {
  return GetImpl()->GetSigninProfile();
}

// static
std::string ProfileHelper::GetUserIdHashFromProfile(const Profile* profile) {
  return BrowserContextHelper::GetUserIdHashFromBrowserContext(
      const_cast<Profile*>(profile));
}

// static
base::FilePath ProfileHelper::GetUserProfileDir(
    const std::string& user_id_hash) {
  return base::FilePath(
      BrowserContextHelper::GetUserBrowserContextDirName(user_id_hash));
}

// static
bool ProfileHelper::IsSigninProfile(const Profile* profile) {
  return ::IsSigninProfile(profile);
}

// static
bool ProfileHelper::IsSigninProfileInitialized() {
  return GetImpl()->IsSigninProfileInitialized();
}

// static
bool ProfileHelper::IsLockScreenAppProfile(const Profile* profile) {
  return ::IsLockScreenAppProfile(profile);
}

// static
base::FilePath ProfileHelper::GetLockScreenAppProfilePath() {
  return GetImpl()->GetProfileDir(chrome::kLockScreenAppProfile);
}

// static
std::string ProfileHelper::GetLockScreenAppProfileName() {
  return chrome::kLockScreenAppProfile;
}

// static
base::FilePath ProfileHelper::GetLockScreenProfileDir() {
  return GetImpl()->GetProfileDir(chrome::kLockScreenProfile);
}

// static
Profile* ProfileHelper::GetLockScreenProfile() {
  return GetImpl()->GetLockScreenProfile();
}

// static
bool ProfileHelper::IsLockScreenProfile(const Profile* profile) {
  return ::IsLockScreenProfile(profile);
}

// static
bool ProfileHelper::IsOwnerProfile(const Profile* profile) {
  if (!profile)
    return false;
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;
  return user->GetAccountId() ==
         user_manager::UserManager::Get()->GetOwnerAccountId();
}

// static
bool ProfileHelper::IsPrimaryProfile(const Profile* profile) {
  if (!profile)
    return false;
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;
  return user == user_manager::UserManager::Get()->GetPrimaryUser();
}

// static
bool ProfileHelper::IsEphemeralUserProfile(const Profile* profile) {
  if (!profile)
    return false;

  // Owner profile is always persistent.
  if (IsOwnerProfile(profile))
    return false;

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  // Guest and public account is ephemeral.
  const user_manager::UserType user_type = user->GetType();
  if (user_type == user_manager::USER_TYPE_GUEST ||
      user_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    return true;
  }

  // Otherwise, users are ephemeral when the policy is enabled.
  return user_manager::UserManager::Get()->AreEphemeralUsersEnabled();
}

// static
bool ProfileHelper::IsUserProfile(const Profile* profile) {
  return ::IsUserProfile(profile);
}

// static
bool ProfileHelper::IsUserProfilePath(const base::FilePath& profile_path) {
  return ::IsUserProfilePath(profile_path);
}

// static
void ProfileHelper::SetProfileToUserForTestingEnabled(bool enabled) {
  enable_profile_to_user_testing = enabled;
}

// static
void ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(bool value) {
  always_return_primary_user_for_testing = value;
  ProfileHelper::SetProfileToUserForTestingEnabled(value);
}

ProfileHelperImpl::ProfileHelperImpl(
    std::unique_ptr<BrowserContextHelper::Delegate> delegate)
    : browser_context_helper_(
          std::make_unique<BrowserContextHelper>(std::move(delegate))) {}

ProfileHelperImpl::~ProfileHelperImpl() = default;

base::FilePath ProfileHelperImpl::GetProfileDir(base::StringPiece profile) {
  const base::FilePath* user_data_dir =
      browser_context_helper_->delegate()->GetUserDataDir();
  if (!user_data_dir)
    return base::FilePath();
  return user_data_dir->AppendASCII(profile);
}

bool ProfileHelperImpl::IsSigninProfileInitialized() {
  return browser_context_helper_->delegate()->GetBrowserContextByPath(
      GetSigninProfileDir());
}

Profile* ProfileHelperImpl::GetSigninProfile() {
  Profile* profile = Profile::FromBrowserContext(
      browser_context_helper_->delegate()->DeprecatedGetBrowserContext(
          GetSigninProfileDir()));
  if (!profile)
    return nullptr;
  return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
}

Profile* ProfileHelperImpl::GetLockScreenProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  Profile* profile =
      profile_manager->GetProfileByPath(GetLockScreenProfileDir());
  if (!profile)
    return nullptr;
  return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
}

base::FilePath ProfileHelperImpl::GetActiveUserProfileDir() {
  return ProfileHelper::GetUserProfileDir(active_user_id_hash_);
}

void ProfileHelperImpl::Initialize() {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

Profile* ProfileHelperImpl::GetProfileByAccountId(const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  if (!user) {
    LOG(WARNING) << "Unable to retrieve user for account_id.";
    return nullptr;
  }

  return GetProfileByUser(user);
}

Profile* ProfileHelperImpl::GetProfileByUser(const user_manager::User* user) {
  // This map is non-empty only in tests.
  if (!user_to_profile_for_testing_.empty()) {
    std::map<const user_manager::User*, Profile*>::const_iterator it =
        user_to_profile_for_testing_.find(user);
    if (it != user_to_profile_for_testing_.end())
      return it->second;
  }

  if (!user->is_profile_created())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(
      browser_context_helper_->delegate()->GetBrowserContextByPath(
          browser_context_helper_->GetBrowserContextPathByUserIdHash(
              user->username_hash())));

  // GetActiveUserProfile() or GetProfileByUserIdHash() returns a new instance
  // of ProfileImpl(), but actually its off-the-record profile should be used.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  return profile;
}

const user_manager::User* ProfileHelperImpl::GetUserByProfile(
    const Profile* profile) const {
  if (!ProfileHelper::IsUserProfile(profile)) {
    return nullptr;
  }

  // This map is non-empty only in tests.
  if (enable_profile_to_user_testing || !user_list_for_testing_.empty()) {
    if (always_return_primary_user_for_testing)
      return user_manager::UserManager::Get()->GetPrimaryUser();

    const std::string& user_name = profile->GetProfileUserName();
    for (user_manager::UserList::const_iterator it =
             user_list_for_testing_.begin();
         it != user_list_for_testing_.end(); ++it) {
      if ((*it)->GetAccountId().GetUserEmail() == user_name)
        return *it;
    }

    // In case of test setup we should always default to primary user.
    return user_manager::UserManager::Get()->GetPrimaryUser();
  }

  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Special case for non-CrOS tests that do create several profiles
  // and don't really care about mapping to the real user.
  // Without multi-profiles on Chrome OS such tests always got active_user_.
  // Now these tests will specify special flag to continue working.
  // In future those tests can get a proper CrOS configuration i.e. register
  // and login several users if they want to work with an additional profile.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreUserProfileMappingForTests)) {
    return user_manager->GetActiveUser();
  }

  // Finds the matching user in logged-in user list since only a logged-in
  // user would have a profile.
  const std::string username_hash =
      ProfileHelper::GetUserIdHashFromProfile(profile);
  const user_manager::UserList& users = user_manager->GetLoggedInUsers();
  const user_manager::UserList::const_iterator pos =
      base::ranges::find_if(users, UsernameHashMatcher(username_hash));
  if (pos != users.end())
    return *pos;

  // Many tests do not have their users registered with UserManager and
  // runs here. If |active_user_| matches |profile|, returns it.
  const user_manager::User* active_user = user_manager->GetActiveUser();
  return active_user &&
                 browser_context_helper_->GetBrowserContextPathByUserIdHash(
                     active_user->username_hash()) == profile->GetPath()
             ? active_user
             : nullptr;
}

user_manager::User* ProfileHelperImpl::GetUserByProfile(
    Profile* profile) const {
  return const_cast<user_manager::User*>(
      GetUserByProfile(static_cast<const Profile*>(profile)));
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, UserManager::UserSessionStateObserver implementation:

void ProfileHelperImpl::ActiveUserHashChanged(const std::string& hash) {
  active_user_id_hash_ = hash;
}

void ProfileHelperImpl::SetProfileToUserMappingForTesting(
    user_manager::User* user) {
  user_list_for_testing_.push_back(user);
}

void ProfileHelperImpl::SetUserToProfileMappingForTesting(
    const user_manager::User* user,
    Profile* profile) {
  user_to_profile_for_testing_[user] = profile;
}

void ProfileHelperImpl::RemoveUserFromListForTesting(
    const AccountId& account_id) {
  auto it = base::ranges::find(user_list_for_testing_, account_id,
                               &user_manager::User::GetAccountId);
  if (it != user_list_for_testing_.end())
    user_list_for_testing_.erase(it);
}

void ProfileHelperImpl::FlushProfile(Profile* profile) {
  if (!profile_flusher_)
    profile_flusher_ = std::make_unique<FileFlusher>();

  // Flushes files directly under profile path since these are the critical
  // ones.
  profile_flusher_->RequestFlush(profile->GetPath(), /*recursive=*/false,
                                 base::OnceClosure());
}

}  // namespace ash
