// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/user_selection_screen.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_utils.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/device_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

const char kWakeLockReason[] = "TPMLockedIssue";
const int kWaitingOvertimeInSeconds = 1;

// Max number of users to show.
// Please keep synced with one in signin_userlist_unittest.cc.
const size_t kMaxUsers = 50;

// Returns true if we have enterprise domain information.
// `out_manager`:  Output value of the manager of the device's domain. Can be
// either a domain (foo.com) or an email address (user@foo.com)
bool GetDeviceManager(std::string* out_manager) {
  policy::BrowserPolicyConnectorAsh* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (policy_connector->IsCloudManaged()) {
    *out_manager = policy_connector->GetEnterpriseDomainManager();
    return true;
  }
  return false;
}

// Get locales information of public account user.
// Returns a list of available locales.
// `public_session_recommended_locales`: This can be nullptr if we don't have
// recommended locales.
// `out_selected_locale`: Output value of the initially selected locale.
// `out_multiple_locales`: Output value indicates whether we have multiple
// recommended locales.
base::Value::List GetPublicSessionLocales(
    const std::vector<std::string>* public_session_recommended_locales,
    std::string* out_selected_locale,
    bool* out_multiple_locales) {
  std::vector<std::string> kEmptyRecommendedLocales;
  const std::vector<std::string>& recommended_locales =
      public_session_recommended_locales ? *public_session_recommended_locales
                                         : kEmptyRecommendedLocales;

  // Construct the list of available locales. This list consists of the
  // recommended locales, followed by all others.
  auto available_locales =
      GetUILanguageList(&recommended_locales, std::string(),
                        input_method::InputMethodManager::Get());

  // Select the the first recommended locale that is actually available or the
  // current UI locale if none of them are available.
  *out_selected_locale =
      FindMostRelevantLocale(recommended_locales, available_locales,
                             g_browser_process->GetApplicationLocale());

  *out_multiple_locales = recommended_locales.size() >= 2;
  return available_locales;
}

// Returns true if dircrypto migration check should be performed.
// TODO(achuith): Get rid of this function altogether.
bool ShouldCheckNeedDircryptoMigration() {
  return arc::IsArcAvailable();
}

// Returns true if the user can run ARC based on the user type.
bool IsUserAllowedForARC(const AccountId& account_id) {
  return user_manager::UserManager::IsInitialized() &&
         arc::IsArcAllowedForUser(
             user_manager::UserManager::Get()->FindUser(account_id));
}

AccountId GetOwnerAccountId() {
  std::string owner_email;
  CrosSettings::Get()->GetString(kDeviceOwner, &owner_email);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId owner = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  return owner;
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
  if (is_single_user && !ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }
  if (!user->GetAccountId().is_valid()) {
    return false;
  }
  if (user->GetAccountId() == GetOwnerAccountId()) {
    return false;
  }
  if (user->GetType() == user_manager::UserType::kPublicAccount ||
      user->is_logged_in() || IsSigninToAdd()) {
    return false;
  }

  return true;
}

// Returns a pair of 1) whether it is allowed to be part of the current
// multi user sign-in session, and 2) that policy for the user.
std::tuple<bool, user_manager::MultiUserSignInPolicy> GetMultiUserSignInPolicy(
    const user_manager::User* user) {
  const std::string& user_id = user->GetAccountId().GetUserEmail();
  user_manager::MultiUserSignInPolicyController* controller =
      user_manager::UserManager::Get()->GetMultiUserSignInPolicyController();
  return {
      controller->IsUserAllowedInSession(user_id),
      controller->GetCachedValue(user_id),
  };
}

// Determines if user auth status requires online sign in.
proximity_auth::mojom::AuthType GetInitialUserAuthType(
    const user_manager::User* user) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSkipForceOnlineSignInForTesting)) {
    return proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD;
  }

  // Public sessions are always allowed to log in offline.
  // Deprecated supervised users are always allowed to log in offline.
  // For all other users, force online sign in if:
  // * The flag to force online sign-in is set for the user.
  // * The user's OAuth token is invalid or unknown.
  if (user->is_logged_in()) {
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  }

  const user_manager::User::OAuthTokenStatus token_status =
      user->oauth_token_status();
  const bool is_public_session =
      user->GetType() == user_manager::UserType::kPublicAccount;
  const bool has_gaia_account = user->HasGaiaAccount();

  if (is_public_session) {
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  }

  // At this point the reason for invalid token should be already set. If not,
  // this might be a leftover from an old version.
  if (has_gaia_account &&
      token_status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
    RecordReauthReason(user->GetAccountId(), ReauthReason::kOther);
  }

  // We need to force an online signin if the user is marked as requiring it or
  // if there's an invalid OAUTH token that needs to be refreshed.
  if (user->force_online_signin()) {
    VLOG(1) << "Online login forced by user flag";
    return proximity_auth::mojom::AuthType::ONLINE_SIGN_IN;
  }

  if (has_gaia_account &&
      (token_status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID ||
       token_status == user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN)) {
    VLOG(1) << "Online login forced due to invalid OAuth2 token status: "
            << token_status;
    return proximity_auth::mojom::AuthType::ONLINE_SIGN_IN;
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::optional<base::TimeDelta> offline_signin_time_limit =
      known_user.GetOfflineSigninLimit(user->GetAccountId());
  if (!offline_signin_time_limit) {
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  }

  const base::Time last_gaia_signin_time =
      known_user.GetLastOnlineSignin(user->GetAccountId());
  if (last_gaia_signin_time == base::Time()) {
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  }
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  const base::TimeDelta time_since_last_gaia_signin =
      now - last_gaia_signin_time;
  if (time_since_last_gaia_signin >= offline_signin_time_limit) {
    return proximity_auth::mojom::AuthType::ONLINE_SIGN_IN;
  }

  return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
}

}  // namespace

// Helper class to call cryptohome to check whether a user needs dircrypto
// migration. The check results are cached to limit calls to cryptohome.
class UserSelectionScreen::DircryptoMigrationChecker {
 public:
  explicit DircryptoMigrationChecker(UserSelectionScreen* owner)
      : owner_(owner) {}

  DircryptoMigrationChecker(const DircryptoMigrationChecker&) = delete;
  DircryptoMigrationChecker& operator=(const DircryptoMigrationChecker&) =
      delete;

  ~DircryptoMigrationChecker() = default;

  // Start to check whether the given user needs dircrypto migration.
  void Check(const AccountId& account_id) {
    focused_user_ = account_id;

    // If the user may be enterprise-managed, don't display the banner, because
    // migration may be blocked by user policy (and user policy is not available
    // at this time yet).
    if (signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
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

    UserDataAuthClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&DircryptoMigrationChecker::RunCryptohomeCheck,
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

    user_data_auth::NeedsDircryptoMigrationRequest request;
    *request.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id);
    UserDataAuthClient::Get()->NeedsDircryptoMigration(
        request, base::BindOnce(&DircryptoMigrationChecker::
                                    OnCryptohomeNeedsDircryptoMigrationCallback,
                                weak_ptr_factory_.GetWeakPtr(), account_id));
  }

  // Callback invoked when NeedsDircryptoMigration call is finished.
  void OnCryptohomeNeedsDircryptoMigrationCallback(
      const AccountId& account_id,
      std::optional<user_data_auth::NeedsDircryptoMigrationReply> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to call cryptohome NeedsDircryptoMigration.";
      // Hide the banner to avoid confusion in http://crbug.com/721948.
      // Cache is not updated so that cryptohome call will still be attempted.
      UpdateUI(account_id, false);
      return;
    }
    bool needs_migration = reply->needs_dircrypto_migration();
    UMA_HISTOGRAM_BOOLEAN("Ash.Login.Login.MigrationBanner", needs_migration);

    needs_dircrypto_migration_cache_[account_id] = needs_migration;
    UpdateUI(account_id, needs_migration);
  }

  // Update UI for the given user when the check result is available.
  void UpdateUI(const AccountId& account_id, bool needs_migration) {
    // Bail if the user is not the currently focused.
    if (account_id != focused_user_) {
      return;
    }

    owner_->ShowBannerMessage(
        needs_migration ? l10n_util::GetStringUTF16(
                              IDS_LOGIN_NEEDS_DIRCRYPTO_MIGRATION_BANNER)
                        : std::u16string(),
        needs_migration);
  }

  const raw_ptr<UserSelectionScreen> owner_;
  AccountId focused_user_ = EmptyAccountId();

  // Cached result of NeedsDircryptoMigration cryptohome check. Key is the
  // account id of users. True value means the user needs dircrypto migration
  // and false means dircrypto migration is done.
  std::map<AccountId, bool> needs_dircrypto_migration_cache_;

  base::WeakPtrFactory<DircryptoMigrationChecker> weak_ptr_factory_{this};
};

// Helper class  to check whether tpm is locked and update UI with time left to
// unlocking.
class UserSelectionScreen::TpmLockedChecker {
 public:
  explicit TpmLockedChecker(UserSelectionScreen* owner) : owner_(owner) {}
  TpmLockedChecker(const TpmLockedChecker&) = delete;
  TpmLockedChecker& operator=(const TpmLockedChecker&) = delete;
  ~TpmLockedChecker() = default;

  void Check() {
    UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
        &TpmLockedChecker::RunCryptohomeCheck, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void RunCryptohomeCheck(bool service_is_ready) {
    if (!service_is_ready) {
      LOG(ERROR) << "Cryptohome is not available.";
      return;
    }

    // Though this function sends D-Bus call to tpm manager, it makes sense to
    // still wait for cryptohome because the the side effect of the lock has to
    // be propagated to cryptohome to cause the known issue of interest.
    chromeos::TpmManagerClient::Get()->GetDictionaryAttackInfo(
        ::tpm_manager::GetDictionaryAttackInfoRequest(),
        base::BindOnce(&TpmLockedChecker::OnGetDictionaryAttackInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Callback invoked when GetDictionaryAttackInfo call is finished.
  void OnGetDictionaryAttackInfo(
      const ::tpm_manager::GetDictionaryAttackInfoReply& reply) {
    check_finised_ = base::TimeTicks::Now();

    if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
      return;
    }

    if (reply.dictionary_attack_lockout_in_effect()) {
      // Add `kWaitingOvertimeInSeconds` for safetiness, i.e hiding UI and
      // releasing `wake_lock_` happens after TPM becomes unlocked.
      dictionary_attack_lockout_time_remaining_ =
          base::Seconds(reply.dictionary_attack_lockout_seconds_remaining() +
                        kWaitingOvertimeInSeconds);
      OnTpmIsLocked();
    } else {
      TpmIsUnlocked();
    }
  }

  void OnTpmIsLocked() {
    AcquireWakeLock();
    clock_ticking_animator_.Start(FROM_HERE, base::Seconds(1), this,
                                  &TpmLockedChecker::UpdateUI);
    tpm_recheck_.Start(FROM_HERE, base::Minutes(1), this,
                       &TpmLockedChecker::Check);
  }

  void UpdateUI() {
    const base::TimeDelta time_spent = base::TimeTicks::Now() - check_finised_;
    if (time_spent > dictionary_attack_lockout_time_remaining_) {
      Check();
    } else {
      owner_->SetTpmLockedState(
          true, dictionary_attack_lockout_time_remaining_ - time_spent);
    }
  }

  void TpmIsUnlocked() {
    clock_ticking_animator_.Stop();
    tpm_recheck_.Stop();
    owner_->SetTpmLockedState(false, base::TimeDelta());
  }

  void AcquireWakeLock() {
    if (!wake_lock_) {
      mojo::Remote<device::mojom::WakeLockProvider> provider;
      content::GetDeviceService().BindWakeLockProvider(
          provider.BindNewPipeAndPassReceiver());
      provider->GetWakeLockWithoutContext(
          device::mojom::WakeLockType::kPreventDisplaySleep,
          device::mojom::WakeLockReason::kOther, kWakeLockReason,
          wake_lock_.BindNewPipeAndPassReceiver());
    }
    // The `wake_lock_` is released once TpmLockedChecker is destroyed.
    // It happens after successful login.
    wake_lock_->RequestWakeLock();
  }

  const raw_ptr<UserSelectionScreen> owner_;

  base::TimeTicks check_finised_;
  base::TimeDelta dictionary_attack_lockout_time_remaining_;

  base::RepeatingTimer clock_ticking_animator_;
  base::RepeatingTimer tpm_recheck_;

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::WeakPtrFactory<TpmLockedChecker> weak_ptr_factory_{this};
};

UserSelectionScreen::UserSelectionScreen(DisplayedScreen display_type)
    : display_type_(display_type) {
  session_manager::SessionManager::Get()->AddObserver(this);
  if (display_type_ != DisplayedScreen::SIGN_IN_SCREEN) {
    return;
  }
  allowed_input_methods_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceLoginScreenInputMethods,
          base::BindRepeating(
              &UserSelectionScreen::OnAllowedInputMethodsChanged,
              base::Unretained(this)));
  OnAllowedInputMethodsChanged();
}

UserSelectionScreen::~UserSelectionScreen() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void UserSelectionScreen::InitEasyUnlock() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(this);
}

void UserSelectionScreen::SetTpmLockedState(bool is_locked,
                                            base::TimeDelta time_left) {
  for (user_manager::User* user : users_) {
    LoginScreen::Get()->GetModel()->SetTpmLockedState(user->GetAccountId(),
                                                      is_locked, time_left);
  }
}

void UserSelectionScreen::SetView(UserBoardView* view) {
  view_ = view;
}

void UserSelectionScreen::Init(const user_manager::UserList& users) {
  users_ = users;

  if (!ime_state_.get()) {
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();
  }

  // Resets observed object in case of re-Init, no-op otherwise.
  scoped_observation_.Reset();
  // Login screen-only  logic to send users through the online re-auth.
  // In-session (including the lock screen) is handled by
  // InSessionPasswordSyncManager.
  if (users.size() > 0 && display_type_ == DisplayedScreen::SIGN_IN_SCREEN) {
    online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(users);
    scoped_observation_.Observe(online_signin_notifier_.get());
    online_signin_notifier_->CheckForPolicyEnforcedOnlineSignin();
    sync_token_checkers_ =
        std::make_unique<PasswordSyncTokenCheckersCollection>();
    sync_token_checkers_->StartPasswordSyncCheckers(users, this);
  } else {
    sync_token_checkers_.reset();
  }

  if (tpm_locked_checker_) {
    return;
  }

  tpm_locked_checker_ = std::make_unique<TpmLockedChecker>(this);
  tpm_locked_checker_->Check();
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
        user->GetType() == user_manager::UserType::kPublicAccount;

    if ((is_public_account && !is_signin_to_add) || is_owner ||
        (!is_public_account && non_owner_count < max_non_owner_users)) {
      if (!is_owner) {
        ++non_owner_count;
      }
      if (is_owner && users_to_send.size() > kMaxUsers) {
        // Owner is always in the list.
        users_to_send.insert(users_to_send.begin() + (kMaxUsers - 1), user);
        while (users_to_send.size() > kMaxUsers) {
          users_to_send.erase(users_to_send.begin() + kMaxUsers);
        }
      } else if (users_to_send.size() < kMaxUsers) {
        users_to_send.push_back(user);
      }
    }
  }
  return users_to_send;
}

void UserSelectionScreen::CheckUserStatus(const AccountId& account_id) {
  // No checks on the multi-profiles signin or locker screen.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }

  if (!token_handle_util_.get()) {
    token_handle_util_ = std::make_unique<TokenHandleUtil>();
  }

  if (token_handle_util_->HasToken(account_id)) {
    token_handle_util_->IsReauthRequired(
        account_id,
        ProfileHelper::Get()->GetSigninProfile()->GetURLLoaderFactory(),
        base::BindOnce(&UserSelectionScreen::OnUserStatusChecked,
                       weak_factory_.GetWeakPtr()));
  }

  // Run dircrypto migration check only on the login screen when necessary.
  if (display_type_ == DisplayedScreen::SIGN_IN_SCREEN &&
      ShouldCheckNeedDircryptoMigration()) {
    if (!dircrypto_migration_checker_) {
      dircrypto_migration_checker_ =
          std::make_unique<DircryptoMigrationChecker>(this);
    }
    dircrypto_migration_checker_->Check(account_id);
  }
}

void UserSelectionScreen::HandleFocusPod(const AccountId& account_id) {
  DCHECK(!pending_focused_account_id_.has_value());
  const session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state == session_manager::SessionState::ACTIVE) {
    // Wait for the session state change before actual work.
    pending_focused_account_id_ = account_id;
    return;
  }
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  if (focused_pod_account_id_ == account_id) {
    return;
  }
  CheckUserStatus(account_id);
  lock_screen_utils::SetUserInputMethod(
      account_id, ime_state_.get(),
      display_type_ ==
          DisplayedScreen::SIGN_IN_SCREEN /* honor_device_policy */);
  lock_screen_utils::SetKeyboardSettings(account_id);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  std::optional<bool> use_24hour_clock =
      known_user.FindBoolPath(account_id, ::prefs::kUse24HourClock);
  if (!use_24hour_clock.has_value()) {
    focused_user_clock_type_.reset();
  } else {
    base::HourClockType clock_type =
        use_24hour_clock.value() ? base::k24HourClock : base::k12HourClock;
    if (focused_user_clock_type_.has_value()) {
      focused_user_clock_type_->UpdateClockType(clock_type);
    } else {
      focused_user_clock_type_ = g_browser_process->platform_part()
                                     ->GetSystemClock()
                                     ->CreateScopedHourClockType(clock_type);
    }
  }

  focused_pod_account_id_ = account_id;
}

void UserSelectionScreen::HandleNoPodFocused() {
  focused_pod_account_id_ = EmptyAccountId();
  focused_user_clock_type_.reset();
  if (display_type_ == DisplayedScreen::SIGN_IN_SCREEN) {
    lock_screen_utils::EnforceDevicePolicyInputMethods(std::string());
  }
}

void UserSelectionScreen::OnAllowedInputMethodsChanged() {
  DCHECK_EQ(display_type_, DisplayedScreen::SIGN_IN_SCREEN);
  if (focused_pod_account_id_.is_valid()) {
    std::string user_input_method_id =
        lock_screen_utils::GetUserLastInputMethodId(focused_pod_account_id_);
    lock_screen_utils::EnforceDevicePolicyInputMethods(user_input_method_id);
  } else {
    lock_screen_utils::EnforceDevicePolicyInputMethods(std::string());
  }
}

void UserSelectionScreen::OnBeforeShow() {
  input_method::InputMethodManager::Get()->SetState(ime_state_);
}

void UserSelectionScreen::OnUserStatusChecked(const AccountId& account_id,
                                              const std::string& token,
                                              bool reauth_required) {
  if (reauth_required) {
    RecordReauthReason(account_id, ReauthReason::kInvalidTokenHandle);
    SetAuthType(account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                std::u16string());
  }
}

// EasyUnlock stuff

void UserSelectionScreen::SetAuthType(const AccountId& account_id,
                                      proximity_auth::mojom::AuthType auth_type,
                                      const std::u16string& initial_value) {
  if (GetAuthType(account_id) ==
      proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD) {
    return;
  }

  DCHECK(GetAuthType(account_id) !=
             proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD ||
         auth_type == proximity_auth::mojom::AuthType::FORCE_OFFLINE_PASSWORD);
  user_auth_type_map_[account_id] = auth_type;

  LoginScreen::Get()->GetModel()->SetTapToUnlockEnabledForUser(
      account_id, auth_type == proximity_auth::mojom::AuthType::USER_CLICK);

  if (auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    LoginScreen::Get()->GetModel()->ForceOnlineSignInForUser(account_id);
  }
}

proximity_auth::mojom::AuthType UserSelectionScreen::GetAuthType(
    const AccountId& account_id) const {
  if (user_auth_type_map_.find(account_id) == user_auth_type_map_.end()) {
    return proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  }
  return user_auth_type_map_.find(account_id)->second;
}

proximity_auth::ScreenlockBridge::LockHandler::ScreenType
UserSelectionScreen::GetScreenType() const {
  switch (display_type_) {
    case DisplayedScreen::LOCK_SCREEN:
      return ScreenType::LOCK_SCREEN;
    default:
      return ScreenType::OTHER_SCREEN;
  }
}

// As of M69, ShowBannerMessage is used only for showing ext4 migration
// warning banner message.
// TODO(fukino): Remove ShowWarningMessage and related implementation along
// with the migration screen once the transition to ext4 is compilete.
void UserSelectionScreen::ShowBannerMessage(const std::u16string& message,
                                            bool is_warning) {
  LoginScreen::Get()->GetModel()->UpdateWarningMessage(message);
}

void UserSelectionScreen::SetSmartLockState(const AccountId& account_id,
                                            SmartLockState state) {
  LoginScreen::Get()->GetModel()->SetSmartLockState(account_id, state);
}

void UserSelectionScreen::NotifySmartLockAuthResult(const AccountId& account_id,
                                                    bool success) {
  LoginScreen::Get()->GetModel()->NotifySmartLockAuthResult(account_id,
                                                            success);
}

void UserSelectionScreen::EnableInput() {
  // TODO(b/271261286): Remove this.
}

void UserSelectionScreen::Unlock(const AccountId& account_id) {
  DCHECK_EQ(GetScreenType(), LOCK_SCREEN);
  ScreenLocker::Hide();
}

void UserSelectionScreen::OnSessionStateChanged() {
  TRACE_EVENT0("login", "UserSelectionScreen::OnSessionStateChanged");
  if (!pending_focused_account_id_.has_value()) {
    return;
  }
  DCHECK(session_manager::SessionManager::Get()->IsUserSessionBlocked());

  AccountId focused_pod(pending_focused_account_id_.value());
  pending_focused_account_id_.reset();
  HandleFocusPod(focused_pod);
}

void UserSelectionScreen::OnInvalidSyncToken(const AccountId& account_id) {
  RecordReauthReason(account_id,
                     ReauthReason::kSamlPasswordSyncTokenValidationFailed);
  SetAuthType(account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
              std::u16string());
}

void UserSelectionScreen::OnOnlineSigninEnforced(const AccountId& account_id) {
  SetAuthType(account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
              std::u16string());
}

void UserSelectionScreen::AttemptEasyUnlock(const AccountId& account_id) {
  SmartLockService* service = GetSmartLockServiceForUser(account_id);
  if (!service) {
    return;
  }
  service->AttemptAuth(account_id);
}

std::vector<LoginUserInfo>
UserSelectionScreen::UpdateAndReturnUserListForAsh() {
  std::vector<LoginUserInfo> user_info_list;

  const AccountId owner = GetOwnerAccountId();
  const bool is_signin_to_add = IsSigninToAdd();
  users_to_send_ = PrepareUserListForSending(users_, owner, is_signin_to_add);

  user_auth_type_map_.clear();

  for (const user_manager::User* user : users_to_send_) {
    const AccountId& account_id = user->GetAccountId();
    bool is_owner = owner == account_id;
    const bool is_public_account =
        user->GetType() == user_manager::UserType::kPublicAccount;
    const proximity_auth::mojom::AuthType initial_auth_type =
        is_public_account
            ? proximity_auth::mojom::AuthType::EXPAND_THEN_USER_CLICK
            : GetInitialUserAuthType(user);
    user_auth_type_map_[account_id] = initial_auth_type;

    LoginUserInfo user_info;
    user_info.basic_user_info.type = user->GetType();
    user_info.basic_user_info.account_id = user->GetAccountId();

    user_manager::KnownUser known_user(g_browser_process->local_state());

    user_info.use_24hour_clock =
        known_user.FindBoolPath(account_id, ::prefs::kUse24HourClock)
            .value_or(base::GetHourClockType() == base::k24HourClock);

    user_info.basic_user_info.display_name =
        base::UTF16ToUTF8(user->GetDisplayName());
    user_info.basic_user_info.display_email = user->display_email();
    user_info.basic_user_info.avatar = ash::BuildAshUserAvatarForUser(*user);
    user_info.auth_type = initial_auth_type;
    user_info.is_signed_in = user->is_logged_in();
    user_info.is_device_owner = is_owner;
    user_info.can_remove = CanRemoveUser(user);
    user_info.fingerprint_state = quick_unlock::GetFingerprintStateForUser(
        user, quick_unlock::Purpose::kUnlock);

    auto* smart_lock_service = GetSmartLockServiceForUser(account_id);
    if (smart_lock_service) {
      user_info.smart_lock_state =
          smart_lock_service->GetInitialSmartLockState();
    }

    user_info.show_pin_pad_for_password = false;
    if (known_user.GetIsEnterpriseManaged(user->GetAccountId()) &&
        user->GetType() != user_manager::UserType::kPublicAccount) {
      if (const std::string* account_manager =
              known_user.GetAccountManager(user->GetAccountId())) {
        user_info.user_account_manager = *account_manager;
      } else {
        user_info.user_account_manager =
            gaia::ExtractDomainName(user->display_email());
      }
    }
    CrosSettings::Get()->GetBoolean(kDeviceShowNumericKeyboardForPassword,
                                    &user_info.show_pin_pad_for_password);
    if (std::optional<bool> show_display_password_button =
            known_user.FindBoolPath(
                user->GetAccountId(),
                prefs::kLoginDisplayPasswordButtonEnabled)) {
      user_info.show_display_password_button =
          show_display_password_button.value();
    }

    // Fill multi-profile data.
    if (!is_signin_to_add) {
      user_info.is_multi_user_sign_in_allowed = true;
    } else {
      std::tie(user_info.is_multi_user_sign_in_allowed,
               user_info.multi_user_sign_in_policy) =
          ash::GetMultiUserSignInPolicy(user);
    }

    // Fill public session data.
    if (user->GetType() == user_manager::UserType::kPublicAccount) {
      std::string manager;
      user_info.public_account_info.emplace();
      if (GetDeviceManager(&manager)) {
        user_info.public_account_info->device_enterprise_manager = manager;
      }

      user_info.public_account_info->using_saml = user->using_saml();

      const std::vector<std::string>* public_session_recommended_locales =
          public_session_recommended_locales_.find(account_id) ==
                  public_session_recommended_locales_.end()
              ? nullptr
              : &public_session_recommended_locales_[account_id];
      std::string selected_locale;
      bool has_multiple_locales;
      auto available_locales =
          GetPublicSessionLocales(public_session_recommended_locales,
                                  &selected_locale, &has_multiple_locales);
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
    if (is_public_account && LoginScreenClientImpl::HasInstance()) {
      LoginScreenClientImpl::Get()->RequestPublicSessionKeyboardLayouts(
          account_id, user_info.public_account_info->default_locale);
    }

    user_info_list.push_back(std::move(user_info));
  }

  return user_info_list;
}

void UserSelectionScreen::SetUsersLoaded(bool loaded) {
  users_loaded_ = loaded;
}

SmartLockService* UserSelectionScreen::GetSmartLockServiceForUser(
    const AccountId& account_id) const {
  if (GetScreenType() == OTHER_SCREEN) {
    return nullptr;
  }

  const user_manager::User* unlock_user = nullptr;
  for (const user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      unlock_user = user;
      break;
    }
  }
  if (!unlock_user) {
    return nullptr;
  }

  ProfileHelper* profile_helper = ProfileHelper::Get();
  Profile* profile = profile_helper->GetProfileByUser(unlock_user);

  // If the active screen is the lock screen, a Profile must exist that is
  // associated with |unlock_user|. This does not apply vice-versa: there are
  // some valid scenarios where |profile| exists but the active screen is not
  // the lock screen.
  if (GetScreenType() == LOCK_SCREEN) {
    DCHECK(profile);
  }

  if (!profile) {
    profile = profile_helper->GetSigninProfile();
  }

  return SmartLockService::Get(profile);
}

}  // namespace ash
