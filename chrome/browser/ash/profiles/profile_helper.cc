// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/profile_helper.h"

#include <set>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/base/file_flusher.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "components/account_id/account_id.h"
#include "components/crx_file/id_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/browser/extension_system.h"

namespace ash {
namespace {

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace login = ::chromeos::login;
using ::chromeos::OAuth2LoginManager;

// This array contains a subset of the explicitly allowlisted extensions that
// are defined in extensions/common/api/_behavior_features.json. The extension
// is treated as risky if it has some UI elements which remain accessible
// after the signin was completed.
const char* kNonRiskyExtensionsIdsHashes[] = {
    "E24F1786D842E91E74C27929B0B3715A4689A473",  // Gnubby component extension
    "6F9E349A0561C78A0D3F41496FE521C5151C7F71",  // Gnubby app
    "06BE211D5F014BAB34BC22D9DDA09C63A81D828E",  // Chrome OS XKB
    "3F50C3A83839D9C76334BCE81CDEC06174F266AF",  // Virtual Keyboard
    "2F47B526FA71F44816618C41EC55E5EE9543FDCC",  // Braille Keyboard
    "86672C8D7A04E24EFB244BF96FE518C4C4809F73",  // Speech synthesis
    "1CF709D51B2B96CF79D00447300BD3BFBE401D21",  // Mobile activation
    "40FF1103292F40C34066E023B8BE8CAE18306EAE",  // Chromeos help
    "3C654B3B6682CA194E75AD044CEDE927675DDEE8",  // Easy unlock
    "75C7F4B720314B6CB1B5817CD86089DB95CD2461",  // ChromeVox
    "4D725C894DA4CF1F4D96C60F0D83BD745EB530CA",  // Switch Access
};

// As defined in /chromeos/dbus/cryptohome/cryptohome_client.cc.
static const char kUserIdHashSuffix[] = "-hash";

bool ShouldAddProfileDirPrefix(const std::string& user_id_hash) {
  // Do not add profile dir prefix for legacy profile dir and test
  // user profile. The reason of not adding prefix for test user profile
  // is to keep the promise that TestingProfile::kTestUserProfileDir and
  // chrome::kTestUserProfileDir are always in sync. Otherwise,
  // TestingProfile::kTestUserProfileDir needs to be dynamically calculated
  // based on whether multi profile is enabled or not.
  return user_id_hash != chrome::kLegacyProfileDir &&
         user_id_hash != chrome::kTestUserProfileDir;
}

void WrapAsBrowsersCloseCallback(const base::RepeatingClosure& callback,
                                 const base::FilePath& path) {
  callback.Run();
}

class UsernameHashMatcher {
 public:
  explicit UsernameHashMatcher(const std::string& h) : username_hash(h) {}
  bool operator()(const user_manager::User* user) const {
    return user->username_hash() == username_hash;
  }

 private:
  const std::string& username_hash;
};

// Internal helper to get an already-loaded user profile by user id hash. Return
// nullptr if the user profile is not yet loaded.
Profile* GetProfileByUserIdHash(const std::string& user_id_hash) {
  return g_browser_process->profile_manager()->GetProfileByPath(
      ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
}

bool IsSigninProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kInitialProfile;
}

bool IsLockScreenAppProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kLockScreenAppProfile;
}

bool IsLockScreenProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kLockScreenProfile;
}

// Returns the path that corresponds to the passed profile.
base::FilePath GetProfileDir(base::StringPiece profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager can be null in unit tests.
  if (!profile_manager)
    return base::FilePath();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  return user_data_dir.AppendASCII(profile);
}

// Returns an incognito profile that corresponds to the passed path.
Profile* GetIncognitoProfile(base::FilePath profile_dir) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // |profile_manager| could be null in tests.
  if (!profile_manager) {
    return nullptr;
  }

  return profile_manager->GetProfile(profile_dir)->GetPrimaryOTRProfile();
}

}  // anonymous namespace

// static
bool ProfileHelper::enable_profile_to_user_testing = false;
bool ProfileHelper::always_return_primary_user_for_testing = false;

class ProfileHelperImpl : public ProfileHelper,
                          public content::BrowsingDataRemover::Observer,
                          public OAuth2LoginManager::Observer {
 public:
  ProfileHelperImpl();
  ~ProfileHelperImpl() override;

  void ProfileStartup(Profile* profile) override;
  base::FilePath GetActiveUserProfileDir() override;
  void Initialize() override;
  void ClearSigninProfile(base::OnceClosure on_clear_callback) override;

  Profile* GetProfileByAccountId(const AccountId& account_id) override;
  Profile* GetProfileByUser(const user_manager::User* user) override;

  Profile* GetProfileByUserUnsafe(const user_manager::User* user) override;

  const user_manager::User* GetUserByProfile(
      const Profile* profile) const override;
  user_manager::User* GetUserByProfile(Profile* profile) const override;

  void SetActiveUserIdForTesting(const std::string& user_id) override;

  void FlushProfile(Profile* profile) override;

  void SetProfileToUserMappingForTesting(user_manager::User* user) override;
  void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                         Profile* profile) override;
  void RemoveUserFromListForTesting(const AccountId& account_id) override;

 private:
  // BrowsingDataRemover::Observer implementation:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  // OAuth2LoginManager::Observer overrides.
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // user_manager::UserManager::UserSessionStateObserver implementation:
  void ActiveUserHashChanged(const std::string& hash) override;

  // Called when signin profile is cleared.
  void OnSigninProfileCleared();

  // Identifies path to active user profile on Chrome OS.
  std::string active_user_id_hash_;

  // List of callbacks called after signin profile clearance.
  std::vector<base::OnceClosure> on_clear_callbacks_;

  // Called when a single stage of profile clearing is finished.
  base::RepeatingClosure on_clear_profile_stage_finished_;

  // A currently running browsing data remover.
  content::BrowsingDataRemover* browsing_data_remover_ = nullptr;

  // Used for testing by unit tests and FakeUserManager/MockUserManager.
  std::map<const user_manager::User*, Profile*> user_to_profile_for_testing_;

  // When this list is not empty GetUserByProfile() will find user that has
  // the same user_id as |profile|->GetProfileName().
  user_manager::UserList user_list_for_testing_;

  std::unique_ptr<FileFlusher> profile_flusher_;

  base::WeakPtrFactory<ProfileHelperImpl> weak_factory_{this};
};

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
  return std::make_unique<ProfileHelperImpl>();
}

// static
ProfileHelper* ProfileHelper::Get() {
  return g_browser_process->platform_part()->profile_helper();
}

// static
Profile* ProfileHelper::GetProfileByUserIdHashForTest(
    const std::string& user_id_hash) {
  base::ScopedAllowBlockingForTesting allow_io;
  return g_browser_process->profile_manager()->GetProfile(
      ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
}

// static
base::FilePath ProfileHelper::GetProfilePathByUserIdHash(
    const std::string& user_id_hash) {
  // Fails if Chrome runs with "--login-manager", but not "--login-profile", and
  // needs to restart. This might happen if you test Chrome OS on Linux and
  // you start a guest session or Chrome crashes. Be sure to add
  //   "--login-profile=user@example.com-hash"
  // to the command line flags.
  DCHECK(!user_id_hash.empty())
      << "user_id_hash is empty, probably need to add "
         "--login-profile=user@example.com-hash to command line parameters";
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = profile_manager->user_data_dir();

  return profile_path.Append(GetUserProfileDir(user_id_hash));
}

// static
base::FilePath ProfileHelper::GetSigninProfileDir() {
  return GetProfileDir(chrome::kInitialProfile);
}

// static
Profile* ProfileHelper::GetSigninProfile() {
  return GetIncognitoProfile(GetSigninProfileDir());
}

// static
std::string ProfileHelper::GetUserIdHashFromProfile(const Profile* profile) {
  if (!profile)
    return std::string();

  std::string profile_dir = profile->GetPath().BaseName().value();

  // Don't strip prefix if the dir is not supposed to be prefixed.
  if (!ShouldAddProfileDirPrefix(profile_dir))
    return profile_dir;

  // Check that profile directory starts with the correct prefix.
  std::string prefix(chrome::kProfileDirPrefix);
  if (!base::StartsWith(profile_dir, prefix, base::CompareCase::SENSITIVE)) {
    // This happens when creating a TestingProfile in browser tests.
    return std::string();
  }

  return profile_dir.substr(prefix.length());
}

// static
base::FilePath ProfileHelper::GetUserProfileDir(
    const std::string& user_id_hash) {
  CHECK(!user_id_hash.empty());
  return ShouldAddProfileDirPrefix(user_id_hash)
             ? base::FilePath(chrome::kProfileDirPrefix + user_id_hash)
             : base::FilePath(user_id_hash);
}

// static
bool ProfileHelper::IsSigninProfile(const Profile* profile) {
  return profile && IsSigninProfilePath(profile->GetPath().BaseName());
}

// static
bool ProfileHelper::IsSigninProfileInitialized() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager &&
         profile_manager->GetProfileByPath(GetSigninProfileDir());
}

// static
bool ProfileHelper::IsLockScreenAppProfile(const Profile* profile) {
  return profile && IsLockScreenAppProfilePath(profile->GetPath().BaseName());
}

// static
base::FilePath ProfileHelper::GetLockScreenAppProfilePath() {
  return GetProfileDir(chrome::kLockScreenAppProfile);
}

// static
std::string ProfileHelper::GetLockScreenAppProfileName() {
  return chrome::kLockScreenAppProfile;
}

// static
base::FilePath ProfileHelper::GetLockScreenProfileDir() {
  return GetProfileDir(chrome::kLockScreenProfile);
}

// static
Profile* ProfileHelper::GetLockScreenIncognitoProfile() {
  return GetIncognitoProfile(GetLockScreenProfileDir());
}

// static
bool ProfileHelper::IsLockScreenProfile(const Profile* profile) {
  return profile && IsLockScreenProfilePath(profile->GetPath().BaseName());
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
  return ChromeUserManager::Get()->AreEphemeralUsersEnabled();
}

// static
bool ProfileHelper::IsRegularProfile(const Profile* profile) {
  return !ProfileHelper::IsSigninProfile(profile) &&
         !ProfileHelper::IsLockScreenAppProfile(profile) &&
         !ProfileHelper::IsLockScreenProfile(profile);
}

// static
bool ProfileHelper::IsRegularProfilePath(const base::FilePath& profile_path) {
  return !IsSigninProfilePath(profile_path) &&
         !IsLockScreenAppProfilePath(profile_path) &&
         !IsLockScreenProfilePath(profile_path);
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

// static
std::string ProfileHelper::GetUserIdHashByUserIdForTesting(
    const std::string& user_id) {
  return user_id + kUserIdHashSuffix;
}

ProfileHelperImpl::ProfileHelperImpl() {}

ProfileHelperImpl::~ProfileHelperImpl() {
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);
}

void ProfileHelperImpl::ProfileStartup(Profile* profile) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the off-the-record profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetPrimaryOTRProfile() call above.
  profile->InitChromeOSPreferences();

  // Add observer so we can see when the first profile's session restore is
  // completed. After that, we won't need the default profile anymore.
  if (!IsSigninProfile(profile) &&
      user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount() &&
      !user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    chromeos::OAuth2LoginManager* login_manager =
        chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
            profile);
    if (login_manager)
      login_manager->AddObserver(this);
  }
}

base::FilePath ProfileHelperImpl::GetActiveUserProfileDir() {
  return ProfileHelper::GetUserProfileDir(active_user_id_hash_);
}

void ProfileHelperImpl::Initialize() {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

void ProfileHelperImpl::ClearSigninProfile(
    base::OnceClosure on_clear_callback) {
  on_clear_callbacks_.push_back(std::move(on_clear_callback));

  // Profile is already clearing.
  if (on_clear_callbacks_.size() > 1)
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Check if signin profile was loaded.
  if (!profile_manager ||
      !profile_manager->GetProfileByPath(GetSigninProfileDir())) {
    OnSigninProfileCleared();
    return;
  }
  on_clear_profile_stage_finished_ = base::BarrierClosure(
      3, base::BindOnce(&ProfileHelperImpl::OnSigninProfileCleared,
                        weak_factory_.GetWeakPtr()));
  LOG_ASSERT(!browsing_data_remover_);
  browsing_data_remover_ =
      content::BrowserContext::GetBrowsingDataRemover(GetSigninProfile());
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES, this);

  // Close the current session with SigninPartitionManager. This clears cached
  // data from the last-used sign-in StoragePartition.
  login::SigninPartitionManager::Factory::GetForBrowserContext(
      GetSigninProfile())
      ->CloseCurrentSigninSession(on_clear_profile_stage_finished_);

  BrowserList::CloseAllBrowsersWithProfile(
      GetSigninProfile(),
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_success */,
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_aborted */,
      true /* skip_beforeunload */);

  // Unload all extensions that could possibly leak the SigninProfile for
  // unauthorized usage.
  // TODO(https://crbug.com/1045929): This also can be fixed by restricting URLs
  //                                  or browser windows from opening.
  const std::set<std::string> allowed_ids_hashes(
      std::begin(kNonRiskyExtensionsIdsHashes),
      std::end(kNonRiskyExtensionsIdsHashes));
  auto* component_loader = extensions::ExtensionSystem::Get(GetSigninProfile())
                               ->extension_service()
                               ->component_loader();
  const std::vector<std::string> loaded_extensions =
      component_loader->GetRegisteredComponentExtensionsIds();
  for (const auto& el : loaded_extensions) {
    const std::string hex_hash = crx_file::id_util::HashedIdInHex(el);
    if (!allowed_ids_hashes.count(hex_hash))
      component_loader->Remove(el);
  }
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
    return NULL;
  Profile* profile = GetProfileByUserIdHash(user->username_hash());

  // GetActiveUserProfile() or GetProfileByUserIdHash() returns a new instance
  // of ProfileImpl(), but actually its off-the-record profile should be used.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetPrimaryOTRProfile();

  return profile;
}

Profile* ProfileHelperImpl::GetProfileByUserUnsafe(
    const user_manager::User* user) {
  // This map is non-empty only in tests.
  if (!user_to_profile_for_testing_.empty()) {
    std::map<const user_manager::User*, Profile*>::const_iterator it =
        user_to_profile_for_testing_.find(user);
    if (it != user_to_profile_for_testing_.end())
      return it->second;
  }

  Profile* profile = NULL;
  if (user->is_profile_created()) {
    profile = GetProfileByUserIdHash(user->username_hash());
  } else {
    LOG(ERROR) << "ProfileHelper::GetProfileByUserUnsafe is called when "
                  "|user|'s profile is not created. It probably means that "
                  "something is wrong with a calling code. Please report in "
                  "http://crbug.com/361528 if you see this message.";
    profile = ProfileManager::GetActiveUserProfile();
  }

  // GetActiveUserProfile() or GetProfileByUserIdHash() returns a new instance
  // of ProfileImpl(), but actually its off-the-record profile should be used.
  if (profile && user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetPrimaryOTRProfile();
  return profile;
}

const user_manager::User* ProfileHelperImpl::GetUserByProfile(
    const Profile* profile) const {
  if (!ProfileHelper::IsRegularProfile(profile)) {
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
  const user_manager::UserList::const_iterator pos = std::find_if(
      users.begin(), users.end(), UsernameHashMatcher(username_hash));
  if (pos != users.end())
    return *pos;

  // Many tests do not have their users registered with UserManager and
  // runs here. If |active_user_| matches |profile|, returns it.
  const user_manager::User* active_user = user_manager->GetActiveUser();
  return active_user && ProfileHelper::GetProfilePathByUserIdHash(
                            active_user->username_hash()) == profile->GetPath()
             ? active_user
             : NULL;
}

user_manager::User* ProfileHelperImpl::GetUserByProfile(
    Profile* profile) const {
  return const_cast<user_manager::User*>(
      GetUserByProfile(static_cast<const Profile*>(profile)));
}

void ProfileHelperImpl::OnSigninProfileCleared() {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(on_clear_callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, content::BrowsingDataRemover::Observer implementation:

void ProfileHelperImpl::OnBrowsingDataRemoverDone(uint64_t failed_data_types) {
  LOG_ASSERT(browsing_data_remover_);
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = nullptr;

  on_clear_profile_stage_finished_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, OAuth2LoginManager::Observer implementation:

void ProfileHelperImpl::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  if (state == OAuth2LoginManager::SESSION_RESTORE_DONE ||
      state == OAuth2LoginManager::SESSION_RESTORE_FAILED ||
      state == OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED) {
    chromeos::OAuth2LoginManager* login_manager =
        chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
            user_profile);
    login_manager->RemoveObserver(this);
    ClearSigninProfile(base::OnceClosure());
  }
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
  auto it =
      std::find_if(user_list_for_testing_.begin(), user_list_for_testing_.end(),
                   [&account_id](const user_manager::User* user) {
                     return user->GetAccountId() == account_id;
                   });
  if (it != user_list_for_testing_.end())
    user_list_for_testing_.erase(it);
}

void ProfileHelperImpl::SetActiveUserIdForTesting(const std::string& user_id) {
  active_user_id_hash_ = GetUserIdHashByUserIdForTesting(user_id);
}

void ProfileHelperImpl::FlushProfile(Profile* profile) {
  if (!profile_flusher_)
    profile_flusher_.reset(new FileFlusher);

  // Flushes files directly under profile path since these are the critical
  // ones.
  profile_flusher_->RequestFlush(profile->GetPath(), /*recursive=*/false,
                                 base::OnceClosure());
}

}  // namespace ash
