// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/views/user_board_view.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"

namespace chromeos {

namespace {

// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyEnterpriseDisplayDomain[] = "enterpriseDisplayDomain";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeyLegacySupervisedUser[] = "legacySupervisedUser";
const char kKeyChildUser[] = "childUser";
const char kKeyDesktopUser[] = "isDesktopUser";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyIsActiveDirectory[] = "isActiveDirectory";
const char kKeyInitialAuthType[] = "initialAuthType";
const char kKeyMultiProfilesAllowed[] = "isMultiProfilesAllowed";
const char kKeyMultiProfilesPolicy[] = "multiProfilesPolicy";
const char kKeyInitialLocales[] = "initialLocales";
const char kKeyInitialLocale[] = "initialLocale";
const char kKeyInitialMultipleRecommendedLocales[] =
    "initialMultipleRecommendedLocales";
const char kKeyAllowFingerprint[] = "allowFingerprint";

// Max number of users to show.
// Please keep synced with one in signin_userlist_unittest.cc.
const size_t kMaxUsers = 18;

const int kPasswordClearTimeoutSec = 60;

// Returns true if we have enterprise domain information.
// |out_domain|:  Output value of the enterprise domain.
bool GetEnterpriseDomain(std::string* out_domain) {
  policy::BrowserPolicyConnectorChromeOS* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (policy_connector->IsCloudManaged()) {
    *out_domain = policy_connector->GetEnterpriseDisplayDomain();
    return true;
  }
  return false;
}

// Get locales information of public account user.
// Returns a list of available locales.
// |public_session_recommended_locales|: This can be nullptr if we don't have
// recommended locales.
// |out_selected_locale|: Output value of the initially selected locale.
// |out_multiple_locales|: Output value indicates whether we have multiple
// recommended locales.
std::unique_ptr<base::ListValue> GetPublicSessionLocales(
    const std::vector<std::string>* public_session_recommended_locales,
    std::string* out_selected_locale,
    bool* out_multiple_locales) {
  std::vector<std::string> kEmptyRecommendedLocales;
  const std::vector<std::string>& recommended_locales =
      public_session_recommended_locales ? *public_session_recommended_locales
                                         : kEmptyRecommendedLocales;

  // Construct the list of available locales. This list consists of the
  // recommended locales, followed by all others.
  std::unique_ptr<base::ListValue> available_locales =
      GetUILanguageList(&recommended_locales, std::string());

  // Select the the first recommended locale that is actually available or the
  // current UI locale if none of them are available.
  *out_selected_locale =
      FindMostRelevantLocale(recommended_locales, *available_locales.get(),
                             g_browser_process->GetApplicationLocale());

  *out_multiple_locales = recommended_locales.size() >= 2;
  return available_locales;
}

void AddPublicSessionDetailsToUserDictionaryEntry(
    base::DictionaryValue* user_dict,
    const std::vector<std::string>* public_session_recommended_locales) {
  std::string domain;
  if (GetEnterpriseDomain(&domain))
    user_dict->SetString(kKeyEnterpriseDisplayDomain, domain);

  std::string selected_locale;
  bool has_multiple_locales;
  std::unique_ptr<base::ListValue> available_locales =
      GetPublicSessionLocales(public_session_recommended_locales,
                              &selected_locale, &has_multiple_locales);

  // Set |kKeyInitialLocales| to the list of available locales.
  user_dict->Set(kKeyInitialLocales, std::move(available_locales));

  // Set |kKeyInitialLocale| to the initially selected locale.
  user_dict->SetString(kKeyInitialLocale, selected_locale);

  // Set |kKeyInitialMultipleRecommendedLocales| to indicate whether the list
  // of recommended locales contains at least two entries. This is used to
  // decide whether the public session pod expands to its basic form (for zero
  // or one recommended locales) or the advanced form (two or more recommended
  // locales).
  user_dict->SetBoolean(kKeyInitialMultipleRecommendedLocales,
                        has_multiple_locales);
}

// Determines the initial fingerprint state for the given user.
ash::FingerprintState GetInitialFingerprintState(
    const user_manager::User* user) {
  // User must be logged in.
  if (!user->is_logged_in())
    return ash::FingerprintState::UNAVAILABLE;

  // Quick unlock storage must be available.
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForUser(user);
  if (!quick_unlock_storage)
    return ash::FingerprintState::UNAVAILABLE;

  // Fingerprint is not registered for this account.
  if (!quick_unlock_storage->fingerprint_storage()->HasRecord())
    return ash::FingerprintState::UNAVAILABLE;

  // Fingerprint unlock attempts should not be exceeded, as the lock screen has
  // not been displayed yet.
  DCHECK(
      !quick_unlock_storage->fingerprint_storage()->ExceededUnlockAttempts());

  // It has been too long since the last authentication.
  if (!quick_unlock_storage->HasStrongAuth())
    return ash::FingerprintState::DISABLED_FROM_TIMEOUT;

  // Auth is available.
  if (quick_unlock_storage->IsFingerprintAuthenticationAvailable())
    return ash::FingerprintState::AVAILABLE;

  // Default to unavailabe.
  return ash::FingerprintState::UNAVAILABLE;
}

// Returns true if dircrypto migration check should be performed.
bool ShouldCheckNeedDircryptoMigration() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableEncryptionMigration) &&
         arc::IsArcAvailable();
}

// Returns true if the user can run ARC based on the user type.
bool IsUserAllowedForARC(const AccountId& account_id) {
  return user_manager::UserManager::IsInitialized() &&
         arc::IsArcAllowedForUser(
             user_manager::UserManager::Get()->FindUser(account_id));
}

AccountId GetOwnerAccountId() {
  std::string owner_email;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner,
                                           &owner_email);
  const AccountId owner = user_manager::known_user::GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  return owner;
}

bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

bool IsSigninToAdd() {
  return LoginDisplayHost::default_host() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}

bool CanRemoveUser(const user_manager::User* user) {
  const bool is_single_user =
      user_manager::UserManager::Get()->GetUsers().size() == 1;

  // Single user check here is necessary because owner info might not be
  // available when running into login screen on first boot.
  // See http://crosbug.com/12723
  if (is_single_user && !IsEnterpriseManaged())
    return false;
  if (!user->GetAccountId().is_valid())
    return false;
  if (user->GetAccountId() == GetOwnerAccountId())
    return false;
  if (user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      user->is_logged_in() || IsSigninToAdd())
    return false;

  return true;
}

void GetMultiProfilePolicy(const user_manager::User* user,
                           bool* out_is_allowed,
                           ash::MultiProfileUserBehavior* out_policy) {
  const std::string& user_id = user->GetAccountId().GetUserEmail();
  MultiProfileUserController* multi_profile_user_controller =
      ChromeUserManager::Get()->GetMultiProfileUserController();
  MultiProfileUserController::UserAllowedInSessionReason is_user_allowed_reason;
  *out_is_allowed = multi_profile_user_controller->IsUserAllowedInSession(
      user_id, &is_user_allowed_reason);

  std::string policy;
  if (is_user_allowed_reason ==
      MultiProfileUserController::NOT_ALLOWED_OWNER_AS_SECONDARY) {
    policy = MultiProfileUserController::kBehaviorOwnerPrimaryOnly;
  } else {
    policy = multi_profile_user_controller->GetCachedValue(user_id);
  }
  *out_policy = MultiProfileUserController::UserBehaviorStringToEnum(policy);
}

}  // namespace

// Helper class to call cryptohome to check whether a user needs dircrypto
// migration. The check results are cached to limit calls to cryptohome.
class UserSelectionScreen::DircryptoMigrationChecker {
 public:
  explicit DircryptoMigrationChecker(UserSelectionScreen* owner)
      : owner_(owner) {}
  ~DircryptoMigrationChecker() = default;

  // Start to check whether the given user needs dircrypto migration.
  void Check(const AccountId& account_id) {
    focused_user_ = account_id;

    // If the user may be enterprise-managed, don't display the banner, because
    // migration may be blocked by user policy (and user policy is not available
    // at this time yet).
    if (!policy::BrowserPolicyConnector::IsNonEnterpriseUser(
            account_id.GetUserEmail())) {
      UpdateUI(account_id, false);
      return;
    }

    auto it = needs_dircrypto_migration_cache_.find(account_id);
    if (it != needs_dircrypto_migration_cache_.end()) {
      UpdateUI(account_id, it->second);
      return;
    }

    // No banner if the user is not allowed for ARC.
    if (!IsUserAllowedForARC(account_id)) {
      UpdateUI(account_id, false);
      return;
    }

    CryptohomeClient::Get()->WaitForServiceToBeAvailable(
        base::Bind(&DircryptoMigrationChecker::RunCryptohomeCheck,
                   weak_ptr_factory_.GetWeakPtr(), account_id));
  }

 private:
  // WaitForServiceToBeAvailable callback to invoke NeedsDircryptoMigration when
  // cryptohome service is available.
  void RunCryptohomeCheck(const AccountId& account_id, bool service_is_ready) {
    if (!service_is_ready) {
      LOG(ERROR) << "Cryptohome is not available.";
      return;
    }

    CryptohomeClient::Get()->NeedsDircryptoMigration(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id),
        base::BindOnce(&DircryptoMigrationChecker::
                           OnCryptohomeNeedsDircryptoMigrationCallback,
                       weak_ptr_factory_.GetWeakPtr(), account_id));
  }

  // Callback invoked when NeedsDircryptoMigration call is finished.
  void OnCryptohomeNeedsDircryptoMigrationCallback(
      const AccountId& account_id,
      base::Optional<bool> needs_migration) {
    if (!needs_migration.has_value()) {
      LOG(ERROR) << "Failed to call cryptohome NeedsDircryptoMigration.";
      // Hide the banner to avoid confusion in http://crbug.com/721948.
      // Cache is not updated so that cryptohome call will still be attempted.
      UpdateUI(account_id, false);
      return;
    }

    needs_dircrypto_migration_cache_[account_id] = needs_migration.value();
    UpdateUI(account_id, needs_migration.value());
  }

  // Update UI for the given user when the check result is available.
  void UpdateUI(const AccountId& account_id, bool needs_migration) {
    // Bail if the user is not the currently focused.
    if (account_id != focused_user_)
      return;

    owner_->ShowBannerMessage(
        needs_migration ? l10n_util::GetStringUTF16(
                              IDS_LOGIN_NEEDS_DIRCRYPTO_MIGRATION_BANNER)
                        : base::string16(),
        needs_migration);
  }

  UserSelectionScreen* const owner_;
  AccountId focused_user_ = EmptyAccountId();

  // Cached result of NeedsDircryptoMigration cryptohome check. Key is the
  // account id of users. True value means the user needs dircrypto migration
  // and false means dircrypto migration is done.
  std::map<AccountId, bool> needs_dircrypto_migration_cache_;

  base::WeakPtrFactory<DircryptoMigrationChecker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DircryptoMigrationChecker);
};

UserSelectionScreen::UserSelectionScreen(const std::string& display_type)
    : BaseScreen(UserBoardView::kScreenId), display_type_(display_type) {}

UserSelectionScreen::~UserSelectionScreen() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

void UserSelectionScreen::InitEasyUnlock() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(this);
}

// static
void UserSelectionScreen::FillUserDictionary(
    const user_manager::User* user,
    bool is_owner,
    bool is_signin_to_add,
    proximity_auth::mojom::AuthType auth_type,
    const std::vector<std::string>* public_session_recommended_locales,
    base::DictionaryValue* user_dict) {
  const bool is_public_session =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
  const bool is_legacy_supervised_user =
      user->GetType() == user_manager::USER_TYPE_SUPERVISED;
  const bool is_child_user = user->GetType() == user_manager::USER_TYPE_CHILD;

  user_dict->SetString(kKeyUsername, user->GetAccountId().Serialize());
  user_dict->SetString(kKeyEmailAddress, user->display_email());
  user_dict->SetString(kKeyDisplayName, user->GetDisplayName());
  user_dict->SetBoolean(kKeyPublicAccount, is_public_session);
  user_dict->SetBoolean(kKeyLegacySupervisedUser, is_legacy_supervised_user);
  user_dict->SetBoolean(kKeyChildUser, is_child_user);
  user_dict->SetBoolean(kKeyDesktopUser, false);
  user_dict->SetInteger(kKeyInitialAuthType, static_cast<int>(auth_type));
  user_dict->SetBoolean(kKeySignedIn, user->is_logged_in());
  user_dict->SetBoolean(kKeyIsOwner, is_owner);
  user_dict->SetBoolean(kKeyIsActiveDirectory, user->IsActiveDirectoryUser());
  user_dict->SetBoolean(
      kKeyAllowFingerprint,
      GetInitialFingerprintState(user) == ash::FingerprintState::AVAILABLE);

  FillMultiProfileUserPrefs(user, user_dict, is_signin_to_add);

  if (is_public_session) {
    AddPublicSessionDetailsToUserDictionaryEntry(
        user_dict, public_session_recommended_locales);
  }
}

// static
void UserSelectionScreen::FillMultiProfileUserPrefs(
    const user_manager::User* user,
    base::DictionaryValue* user_dict,
    bool is_signin_to_add) {
  if (!is_signin_to_add) {
    user_dict->SetBoolean(kKeyMultiProfilesAllowed, true);
    return;
  }

  bool is_user_allowed;
  ash::MultiProfileUserBehavior policy;
  GetMultiProfilePolicy(user, &is_user_allowed, &policy);
  user_dict->SetBoolean(kKeyMultiProfilesAllowed, is_user_allowed);
  user_dict->SetInteger(kKeyMultiProfilesPolicy, static_cast<int>(policy));
}

// static
bool UserSelectionScreen::ShouldForceOnlineSignIn(
    const user_manager::User* user) {
  // Public sessions are always allowed to log in offline.
  // Supervised users are always allowed to log in offline.
  // For all other users, force online sign in if:
  // * The flag to force online sign-in is set for the user.
  // * The user's OAuth token is invalid or unknown.
  if (user->is_logged_in())
    return false;

  const user_manager::User::OAuthTokenStatus token_status =
      user->oauth_token_status();
  const bool is_supervised_user =
      user->GetType() == user_manager::USER_TYPE_SUPERVISED;
  const bool is_public_session =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
  const bool has_gaia_account = user->HasGaiaAccount();

  if (is_supervised_user)
    return false;

  if (is_public_session)
    return false;

  // At this point the reason for invalid token should be already set. If not,
  // this might be a leftover from an old version.
  if (has_gaia_account &&
      token_status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID)
    RecordReauthReason(user->GetAccountId(), ReauthReason::OTHER);

  // We need to force an online signin if the user is marked as requiring it or
  // if there's an invalid OAUTH token that needs to be refreshed.
  if (user->force_online_signin()) {
    VLOG(1) << "Online login forced by user flag";
    return true;
  }

  if (has_gaia_account &&
      (token_status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID ||
       token_status == user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN)) {
    VLOG(1) << "Online login forced due to invalid OAuth2 token status: "
            << token_status;
    return true;
  }

  return false;
}

// static
ash::UserAvatar UserSelectionScreen::BuildAshUserAvatarForUser(
    const user_manager::User& user) {
  ash::UserAvatar avatar;
  avatar.image = user.GetImage();
  if (avatar.image.isNull()) {
    avatar.image = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_LOGIN_DEFAULT_USER);
  }

  // TODO(jdufault): Unify image handling between this code and
  // user_image_source::GetUserImageInternal.
  auto load_image_from_resource = [&avatar](int resource_id) {
    auto& rb = ui::ResourceBundle::GetSharedInstance();
    base::StringPiece avatar_data =
        rb.GetRawDataResourceForScale(resource_id, rb.GetMaxScaleFactor());
    avatar.bytes.assign(avatar_data.begin(), avatar_data.end());
  };
  if (user.has_image_bytes()) {
    avatar.bytes.assign(
        user.image_bytes()->front(),
        user.image_bytes()->front() + user.image_bytes()->size());
  } else if (user.HasDefaultImage()) {
    int resource_id = chromeos::default_user_image::kDefaultImageResourceIDs
        [user.image_index()];
    load_image_from_resource(resource_id);
  } else if (user.image_is_stub()) {
    load_image_from_resource(IDR_LOGIN_DEFAULT_USER);
  }

  return avatar;
}

void UserSelectionScreen::SetHandler(LoginDisplayWebUIHandler* handler) {
  handler_ = handler;

  if (handler_) {
    // Forcibly refresh all of the user images, as the |handler_| instance may
    // have been reused.
    for (user_manager::User* user : users_)
      handler_->OnUserImageChanged(*user);
  }
}

void UserSelectionScreen::SetView(UserBoardView* view) {
  view_ = view;
}

void UserSelectionScreen::Init(const user_manager::UserList& users) {
  users_ = users;

  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && !activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

void UserSelectionScreen::OnBeforeUserRemoved(const AccountId& account_id) {
  for (auto it = users_.cbegin(); it != users_.cend(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      users_.erase(it);
      break;
    }
  }
}

void UserSelectionScreen::OnUserRemoved(const AccountId& account_id) {
  if (!handler_)
    return;
  handler_->OnUserRemoved(account_id, users_.empty());
}

void UserSelectionScreen::OnUserImageChanged(const user_manager::User& user) {
  if (!handler_)
    return;
  handler_->OnUserImageChanged(user);
  // TODO(antrim) : updateUserImage(user.email())
}

void UserSelectionScreen::OnPasswordClearTimerExpired() {
  if (handler_)
    handler_->ClearUserPodPassword();
}

void UserSelectionScreen::OnUserActivity(const ui::Event* event) {
  if (!password_clear_timer_.IsRunning()) {
    password_clear_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kPasswordClearTimeoutSec), this,
        &UserSelectionScreen::OnPasswordClearTimerExpired);
  }
  password_clear_timer_.Reset();
}

// static
const user_manager::UserList UserSelectionScreen::PrepareUserListForSending(
    const user_manager::UserList& users,
    const AccountId& owner,
    bool is_signin_to_add) {
  user_manager::UserList users_to_send;
  bool has_owner = owner.is_valid();
  size_t max_non_owner_users = has_owner ? kMaxUsers - 1 : kMaxUsers;
  size_t non_owner_count = 0;

  for (user_manager::User* user : users) {
    bool is_owner = user->GetAccountId() == owner;
    bool is_public_account =
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;

    if ((is_public_account && !is_signin_to_add) || is_owner ||
        (!is_public_account && non_owner_count < max_non_owner_users)) {
      if (!is_owner)
        ++non_owner_count;
      if (is_owner && users_to_send.size() > kMaxUsers) {
        // Owner is always in the list.
        users_to_send.insert(users_to_send.begin() + (kMaxUsers - 1), user);
        while (users_to_send.size() > kMaxUsers)
          users_to_send.erase(users_to_send.begin() + kMaxUsers);
      } else if (users_to_send.size() < kMaxUsers) {
        users_to_send.push_back(user);
      }
    }
  }
  return users_to_send;
}

void UserSelectionScreen::SendUserList() {
  std::unique_ptr<base::ListValue> users_list =
      UpdateAndReturnUserListForWebUI();
  handler_->LoadUsers(users_to_send_, *users_list);
}

void UserSelectionScreen::HandleGetUsers() {
  SendUserList();
}

void UserSelectionScreen::CheckUserStatus(const AccountId& account_id) {
  // No checks on the multi-profiles signin or locker screen.
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  if (!token_handle_util_.get()) {
    token_handle_util_.reset(new TokenHandleUtil());
  }

  if (token_handle_util_->HasToken(account_id)) {
    token_handle_util_->CheckToken(
        account_id, base::Bind(&UserSelectionScreen::OnUserStatusChecked,
                               weak_factory_.GetWeakPtr()));
  }

  // Run dircrypto migration check only on the login screen when necessary.
  if (display_type_ == OobeUI::kLoginDisplay &&
      ShouldCheckNeedDircryptoMigration()) {
    if (!dircrypto_migration_checker_) {
      dircrypto_migration_checker_ =
          std::make_unique<DircryptoMigrationChecker>(this);
    }
    dircrypto_migration_checker_->Check(account_id);
  }
}

void UserSelectionScreen::OnUserStatusChecked(
    const AccountId& account_id,
    TokenHandleUtil::TokenHandleStatus status) {
  if (status == TokenHandleUtil::INVALID) {
    RecordReauthReason(account_id, ReauthReason::INVALID_TOKEN_HANDLE);
    token_handle_util_->MarkHandleInvalid(account_id);
    SetAuthType(account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                base::string16());
  }
}

// EasyUnlock stuff

void UserSelectionScreen::SetAuthType(const AccountId& account_id,
                                      proximity_auth::mojom::AuthType auth_type,
                                      const base::string16& initial_value) {
  if (GetAuthType(account_id) ==
      proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD) {
    return;
  }

  DCHECK(GetAuthType(account_id) !=
             proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD ||
         auth_type == proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD);
  user_auth_type_map_[account_id] = auth_type;
  view_->SetAuthType(account_id, auth_type, initial_value);
}

proximity_auth::mojom::AuthType UserSelectionScreen::GetAuthType(
    const AccountId& account_id) const {
  if (user_auth_type_map_.find(account_id) == user_auth_type_map_.end())
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  return user_auth_type_map_.find(account_id)->second;
}

proximity_auth::ScreenlockBridge::LockHandler::ScreenType
UserSelectionScreen::GetScreenType() const {
  if (display_type_ == OobeUI::kLockDisplay)
    return LOCK_SCREEN;

  if (display_type_ == OobeUI::kLoginDisplay)
    return SIGNIN_SCREEN;

  return OTHER_SCREEN;
}

void UserSelectionScreen::ShowBannerMessage(const base::string16& message,
                                            bool is_warning) {
  view_->ShowBannerMessage(message, is_warning);
}

void UserSelectionScreen::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  view_->ShowUserPodCustomIcon(account_id, icon_options);
}

void UserSelectionScreen::HideUserPodCustomIcon(const AccountId& account_id) {
  view_->HideUserPodCustomIcon(account_id);
}

void UserSelectionScreen::EnableInput() {
  // If Easy Unlock fails to unlock the screen, re-enable the password input.
  // This is only necessary on the lock screen, because the error handling for
  // the sign-in screen uses a different code path.
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->EnableInput();
}

void UserSelectionScreen::Unlock(const AccountId& account_id) {
  DCHECK_EQ(GetScreenType(), LOCK_SCREEN);
  ScreenLocker::Hide();
}

void UserSelectionScreen::AttemptEasySignin(const AccountId& account_id,
                                            const std::string& secret,
                                            const std::string& key_label) {
  DCHECK_EQ(GetScreenType(), SIGNIN_SCREEN);

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetAuthFlow(UserContext::AUTH_FLOW_EASY_UNLOCK);
  user_context.SetKey(Key(secret));
  user_context.GetKey()->SetLabel(key_label);

  // LoginDisplayHost does not exist in views-based lock screen.
  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->GetLoginDisplay()->delegate()->Login(
        user_context, SigninSpecifics());
  } else {
    SmartLockMetricsRecorder::RecordAuthResultSignInFailure(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kLoginDisplayHostDoesNotExist);
  }
}

void UserSelectionScreen::Show() {}

void UserSelectionScreen::Hide() {}

void UserSelectionScreen::HardLockPod(const AccountId& account_id) {
  view_->SetAuthType(account_id,
                     proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
                     base::string16());
  EasyUnlockService* service = GetEasyUnlockServiceForUser(account_id);
  if (!service)
    return;
  service->SetHardlockState(EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
}

void UserSelectionScreen::AttemptEasyUnlock(const AccountId& account_id) {
  EasyUnlockService* service = GetEasyUnlockServiceForUser(account_id);
  if (!service)
    return;
  service->AttemptAuth(account_id);
}

std::unique_ptr<base::ListValue>
UserSelectionScreen::UpdateAndReturnUserListForWebUI() {
  std::unique_ptr<base::ListValue> users_list =
      std::make_unique<base::ListValue>();

  // TODO(nkostylev): Move to a separate method in UserManager.
  // http://crbug.com/230852
  const AccountId owner = GetOwnerAccountId();
  const bool is_signin_to_add = IsSigninToAdd();

  users_to_send_ = PrepareUserListForSending(users_, owner, is_signin_to_add);

  user_auth_type_map_.clear();

  for (const user_manager::User* user : users_to_send_) {
    const AccountId& account_id = user->GetAccountId();
    bool is_owner = (account_id == owner);
    const bool is_public_account =
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    const proximity_auth::mojom::AuthType initial_auth_type =
        is_public_account
            ? proximity_auth::mojom::AuthType::EXPAND_THEN_USER_CLICK
            : (ShouldForceOnlineSignIn(user)
                   ? proximity_auth::mojom::AuthType::ONLINE_SIGN_IN
                   : proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);
    user_auth_type_map_[account_id] = initial_auth_type;

    auto user_dict = std::make_unique<base::DictionaryValue>();
    const std::vector<std::string>* public_session_recommended_locales =
        public_session_recommended_locales_.find(account_id) ==
                public_session_recommended_locales_.end()
            ? nullptr
            : &public_session_recommended_locales_[account_id];
    FillUserDictionary(user, is_owner, is_signin_to_add, initial_auth_type,
                       public_session_recommended_locales, user_dict.get());
    user_dict->SetBoolean(kKeyCanRemove, CanRemoveUser(user));
    users_list->Append(std::move(user_dict));
  }

  return users_list;
}

std::vector<ash::LoginUserInfo>
UserSelectionScreen::UpdateAndReturnUserListForAsh() {
  std::vector<ash::LoginUserInfo> user_info_list;

  const AccountId owner = GetOwnerAccountId();
  const bool is_signin_to_add = IsSigninToAdd();
  users_to_send_ = PrepareUserListForSending(users_, owner, is_signin_to_add);

  user_auth_type_map_.clear();

  for (const user_manager::User* user : users_to_send_) {
    const AccountId& account_id = user->GetAccountId();
    bool is_owner = owner == account_id;
    const bool is_public_account =
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    const proximity_auth::mojom::AuthType initial_auth_type =
        is_public_account
            ? proximity_auth::mojom::AuthType::EXPAND_THEN_USER_CLICK
            : (ShouldForceOnlineSignIn(user)
                   ? proximity_auth::mojom::AuthType::ONLINE_SIGN_IN
                   : proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);
    user_auth_type_map_[account_id] = initial_auth_type;

    ash::LoginUserInfo user_info;
    user_info.basic_user_info.type = user->GetType();
    user_info.basic_user_info.account_id = user->GetAccountId();
    user_info.basic_user_info.display_name =
        base::UTF16ToUTF8(user->GetDisplayName());
    user_info.basic_user_info.display_email = user->display_email();
    user_info.basic_user_info.avatar = BuildAshUserAvatarForUser(*user);
    user_info.auth_type = initial_auth_type;
    user_info.is_signed_in = user->is_logged_in();
    user_info.is_device_owner = is_owner;
    user_info.can_remove = CanRemoveUser(user);
    user_info.fingerprint_state = GetInitialFingerprintState(user);
    user_info.show_pin_pad_for_password = false;
    chromeos::CrosSettings::Get()->GetBoolean(
        chromeos::kDeviceShowNumericKeyboardForPassword,
        &user_info.show_pin_pad_for_password);

    // Fill multi-profile data.
    if (!is_signin_to_add) {
      user_info.is_multiprofile_allowed = true;
    } else {
      GetMultiProfilePolicy(user, &user_info.is_multiprofile_allowed,
                            &user_info.multiprofile_policy);
    }

    // Fill public session data.
    if (user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      std::string domain;
      user_info.public_account_info.emplace();
      if (GetEnterpriseDomain(&domain))
        user_info.public_account_info->enterprise_domain = domain;

      user_info.public_account_info->using_saml = user->using_saml();

      const std::vector<std::string>* public_session_recommended_locales =
          public_session_recommended_locales_.find(account_id) ==
                  public_session_recommended_locales_.end()
              ? nullptr
              : &public_session_recommended_locales_[account_id];
      std::string selected_locale;
      bool has_multiple_locales;
      std::unique_ptr<base::ListValue> available_locales =
          GetPublicSessionLocales(public_session_recommended_locales,
                                  &selected_locale, &has_multiple_locales);
      DCHECK(available_locales);
      user_info.public_account_info->available_locales =
          lock_screen_utils::FromListValueToLocaleItem(
              std::move(available_locales));
      user_info.public_account_info->default_locale = selected_locale;
      user_info.public_account_info->show_advanced_view = has_multiple_locales;
      // Do not show expanded view when in demo mode.
      user_info.public_account_info->show_expanded_view =
          !DemoSession::IsDeviceInDemoMode();
    }

    user_info.can_remove = CanRemoveUser(user);

    // Send a request to get keyboard layouts for default locale.
    if (is_public_account && LoginScreenClient::HasInstance()) {
      LoginScreenClient::Get()->RequestPublicSessionKeyboardLayouts(
          account_id, user_info.public_account_info->default_locale);
    }

    user_info_list.push_back(std::move(user_info));
  }

  return user_info_list;
}

void UserSelectionScreen::SetUsersLoaded(bool loaded) {
  users_loaded_ = loaded;
}

EasyUnlockService* UserSelectionScreen::GetEasyUnlockServiceForUser(
    const AccountId& account_id) const {
  if (GetScreenType() == OTHER_SCREEN)
    return nullptr;

  const user_manager::User* unlock_user = nullptr;
  for (const user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      unlock_user = user;
      break;
    }
  }
  if (!unlock_user)
    return nullptr;

  ProfileHelper* profile_helper = ProfileHelper::Get();
  Profile* profile = profile_helper->GetProfileByUser(unlock_user);

  // The user profile should exist if and only if this is the lock screen.
  DCHECK_EQ(!!profile, GetScreenType() == LOCK_SCREEN);

  if (!profile)
    profile = profile_helper->GetSigninProfile();

  return EasyUnlockService::Get(profile);
}

}  // namespace chromeos
