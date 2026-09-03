// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker_controller.h"
#include "chrome/browser/ash/login/lock/views_screen_locker.h"
#include "chrome/browser/ash/login/login_auth_recorder.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_utils.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/biod/constants.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "services/device/public/mojom/fingerprint.mojom-shared.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// TODO(b/228873153): Remove after figuring out the root cause of the bug
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

using ::base::UserMetricsAction;

// Returns true if fingerprint authentication is available for `user`.
bool IsFingerprintAvailableForUser(
    quick_unlock::QuickUnlockStorage* quick_unlock_storage) {
  return quick_unlock_storage &&
         quick_unlock_storage->IsFingerprintAuthenticationAvailable(
             quick_unlock::Purpose::kUnlock);
}

const base::Clock* g_clock_for_testing_ = nullptr;
const base::TickClock* g_tick_clock_for_testing_ = nullptr;

chromeos::CertificateProviderService* GetLoginScreenCertProviderService() {
  auto* browser_context =
      BrowserContextHelper::Get()->GetSigninBrowserContext();
  DCHECK(browser_context);
  return chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
      browser_context);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker, public:

ScreenLocker::ScreenLocker(
    PrefService* local_state,
    const ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    const user_manager::MultiUserSignInPolicyController*
        multi_user_sign_in_policy_controller,
    system::SystemClock* system_clock,
    const user_manager::UserList& users)
    : local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      browser_policy_connector_ash_(CHECK_DEREF(browser_policy_connector_ash)),
      multi_user_sign_in_policy_controller_(
          CHECK_DEREF(multi_user_sign_in_policy_controller)),
      system_clock_(CHECK_DEREF(system_clock)),
      users_(users),
      challenge_response_auth_keys_loader_(&local_state_.get()) {
  CHECK(shared_url_loader_factory_);
  CHECK(base::CurrentUIThread::IsSet());

  content::GetDeviceService().BindFingerprint(
      fp_service_.BindNewPipeAndPassReceiver());
  fp_service_->AddFingerprintObserver(
      fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

  GetLoginScreenCertProviderService()->pin_dialog_manager()->AddPinDialogHost(
      &security_token_pin_dialog_host_login_impl_);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  if (g_clock_for_testing_ && g_tick_clock_for_testing_) {
    update_fingerprint_state_timer_ = std::make_unique<base::WallClockTimer>(
        g_clock_for_testing_, g_tick_clock_for_testing_);
  } else {
    update_fingerprint_state_timer_ = std::make_unique<base::WallClockTimer>();
  }
}

void ScreenLocker::Init() {
  VLOG(1) << "ScreenLocker::Init()";
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  saved_ime_state_ = imm->GetActiveIMEState();
  imm->SetState(saved_ime_state_->Clone());
  input_method::InputMethodManager::Get()->GetActiveIMEState()->SetUIStyle(
      input_method::InputMethodManager::UIStyle::kLock);
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->DisableNonLockScreenLayouts();

  authenticator_ = UserSessionManager::GetInstance()->CreateAuthenticator(this);

  // Create ViewScreenLocker that calls into the views-based lock screen via
  // mojo.
  views_screen_locker_ = std::make_unique<ViewsScreenLocker>(
      &local_state_.get(), &application_locale_storage_.get(),
      shared_url_loader_factory_, &browser_policy_connector_ash_.get(),
      &multi_user_sign_in_policy_controller_.get(), &system_clock_.get());

  // Create and display lock screen.
  CHECK(LoginScreenClientImpl::HasInstance());
  LoginScreen::Get()->ShowLockScreen();
  views_screen_locker_->Init(GetUsersToShow());
  ScreenLockReady();

  session_manager::SessionManager::Get()->NotifyLoginOrLockScreenVisible();

  // Start locking on ash side.
  SessionControllerClientImpl::Get()->StartLock(base::BindOnce(
      &ScreenLocker::OnStartLockCallback, weak_factory_.GetWeakPtr()));
}

void ScreenLocker::OnAuthFailure(const AuthFailure& error) {
  base::RecordAction(UserMetricsAction("ScreenLocker_OnLoginFailure"));
  if (authentication_start_time_.is_null()) {
    LOG(ERROR) << "Start time is not set at authentication failure";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication failure: " << delta.InSecondsF() << " second(s)";
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationFailureTime", delta);
  }

  UMA_HISTOGRAM_ENUMERATION("ScreenLocker.AuthenticationFailure",
                            unlock_attempt_type_, UnlockType::AUTH_COUNT);
  session_manager::SessionManager::Get()->NotifyUnlockAttempt(
      /*success*/ false, TransformUnlockType());

  if (auth_status_consumer_for_testing_) {
    auth_status_consumer_for_testing_->OnAuthFailure(error);
  }

  if (pending_auth_state_) {
    GetLoginScreenCertProviderService()
        ->AbortSignatureRequestsForAuthenticatingUser(
            pending_auth_state_->account_id);
    std::move(pending_auth_state_->callback).Run(false);
    pending_auth_state_.reset();
  }
}

void ScreenLocker::OnAuthSuccess(const UserContext& user_context) {
  if (unlock_started_) {
    VLOG(1) << "OnAuthSuccess called while unlock is already runing";
    return;
  }

  unlock_started_ = true;
  CHECK(!IsAuthTemporarilyDisabledForUser(user_context.GetAccountId()))
      << "Authentication is disabled for this user.";

  incorrect_passwords_count_ = 0;
  if (authentication_start_time_.is_null()) {
    if (user_context.GetAccountId().is_valid()) {
      LOG(ERROR) << "Start time is not set at authentication success";
    }
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication success: " << delta.InSecondsF() << " second(s)";
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationSuccessTime", delta);
  }

  UMA_HISTOGRAM_ENUMERATION("ScreenLocker.AuthenticationSuccess",
                            unlock_attempt_type_, UnlockType::AUTH_COUNT);
  auto& session_manager = CHECK_DEREF(session_manager::SessionManager::Get());
  session_manager.NotifyUnlockAttempt(/*success*/ true, TransformUnlockType());

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (user) {
    if (!user->is_active()) {
      saved_ime_state_ = nullptr;
      session_manager.SwitchActiveSession(user_context.GetAccountId());
    }

    // Reset the number of PIN attempts available to the user. We always do this
    // because:
    // 1. If the user signed in with a PIN, that means they should be able to
    //    continue signing in with a PIN.
    // 2. If the user signed in with cryptohome keys, then the PIN timeout is
    //    going to be reset as well, so it is safe to reset the unlock attempt
    //    count.
    quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        quick_unlock::QuickUnlockFactory::GetForUser(user);
    if (quick_unlock_storage) {
      if (unlock_attempt_type_ == AUTH_PASSWORD ||
          unlock_attempt_type_ == AUTH_CHALLENGE_RESPONSE) {
        quick_unlock_storage->MarkStrongAuth();
      }
      quick_unlock_storage->pin_storage_prefs()->ResetUnlockAttemptCount();
      quick_unlock_storage->fingerprint_storage()->ResetUnlockAttemptCount();
    }
  } else {
    NOTREACHED() << "Logged in user not found.";
  }

  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(true);
    pending_auth_state_.reset();
  }

  if (auth_status_consumer_for_testing_) {
    auth_status_consumer_for_testing_->OnAuthSuccess(user_context);
  }
  weak_factory_.InvalidateWeakPtrs();

  VLOG(1) << "Hiding the lock screen.";
  // TODO(crbug.com/539761804): Avoid circular dependency.
  ScreenLockerController::Get().HideLockScreen();
}

void ScreenLocker::ReenableAuthForUser(const AccountId& account_id) {
  if (!IsAuthTemporarilyDisabledForUser(account_id)) {
    return;
  }

  const user_manager::User* user = FindUnlockUser(account_id);
  CHECK(user) << "Invalid user - cannot enable authentication.";

  users_with_temporarily_disabled_auth_.erase(account_id);
  LoginScreen::Get()->GetModel()->EnableAuthForUser(account_id);
}

void ScreenLocker::TemporarilyDisableAuthForUser(
    const AccountId& account_id,
    const AuthDisabledData& auth_disabled_data) {
  if (IsAuthTemporarilyDisabledForUser(account_id)) {
    return;
  }

  const user_manager::User* user = FindUnlockUser(account_id);
  CHECK(user) << "Invalid user - cannot disable authentication.";

  users_with_temporarily_disabled_auth_.insert(account_id);
  LoginScreen::Get()->GetModel()->DisableAuthForUser(account_id,
                                                     auth_disabled_data);
}

void ScreenLocker::Authenticate(std::unique_ptr<UserContext> user_context,
                                AuthenticateCallback callback) {
  LOG_ASSERT(IsUserLoggedIn(user_context->GetAccountId()))
      << "Invalid user trying to unlock.";

  // Do not attempt authentication if it is disabled for the user.
  if (IsAuthTemporarilyDisabledForUser(user_context->GetAccountId())) {
    VLOG(1) << "Authentication disabled for user.";
    if (auth_status_consumer_for_testing_) {
      auth_status_consumer_for_testing_->OnAuthFailure(
          AuthFailure(AuthFailure::AUTH_DISABLED));
    }
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  DCHECK(!pending_auth_state_);
  pending_auth_state_ = std::make_unique<AuthState>(
      user_context->GetAccountId(), std::move(callback));
  unlock_attempt_type_ = AUTH_PASSWORD;

  authentication_start_time_ = base::Time::Now();
  if (user_context->IsUsingPin()) {
    unlock_attempt_type_ = AUTH_PIN;
  }

  const user_manager::User* user = FindUnlockUser(user_context->GetAccountId());
  if (user) {
    // Check to see if the user submitted a PIN and it is valid.
    if (unlock_attempt_type_ == AUTH_PIN) {
      const Key* key = user_context->GetKey();
      CHECK(key);
      quick_unlock::PinBackend::GetInstance()->TryAuthenticate(
          std::move(user_context), *key, quick_unlock::Purpose::kUnlock,
          base::BindOnce(&ScreenLocker::OnPinAttemptDone,
                         weak_factory_.GetWeakPtr()));
      // OnPinAttemptDone will call ContinueAuthenticate.
      return;
    }
  }

  ContinueAuthenticate(std::move(user_context));
}

void ScreenLocker::AuthenticateWithChallengeResponse(
    const AccountId& account_id,
    AuthenticateCallback callback) {
  LOG_ASSERT(IsUserLoggedIn(account_id)) << "Invalid user trying to unlock.";

  // Do not attempt authentication if it is disabled for the user.
  if (IsAuthTemporarilyDisabledForUser(account_id)) {
    VLOG(1) << "Authentication disabled for user.";
    if (auth_status_consumer_for_testing_) {
      auth_status_consumer_for_testing_->OnAuthFailure(
          AuthFailure(AuthFailure::AUTH_DISABLED));
    }
    std::move(callback).Run(false);
    return;
  }

  if (!ChallengeResponseAuthKeysLoader::CanAuthenticateUser(local_state_.get(),
                                                            account_id)) {
    LOG(ERROR)
        << "Challenge-response authentication isn't supported for the user";
    if (auth_status_consumer_for_testing_) {
      auth_status_consumer_for_testing_->OnAuthFailure(
          AuthFailure(AuthFailure::UNLOCK_FAILED));
    }
    std::move(callback).Run(false);
    return;
  }

  DCHECK(!pending_auth_state_);
  pending_auth_state_ =
      std::make_unique<AuthState>(account_id, std::move(callback));

  unlock_attempt_type_ = AUTH_CHALLENGE_RESPONSE;
  challenge_response_auth_keys_loader_.LoadAvailableKeys(
      account_id, base::BindOnce(&ScreenLocker::OnChallengeResponseKeysPrepared,
                                 weak_factory_.GetWeakPtr(), account_id));
  // OnChallengeResponseKeysPrepared will call ContinueAuthenticate.
}

void ScreenLocker::OnChallengeResponseKeysPrepared(
    const AccountId& account_id,
    std::vector<ChallengeResponseKey> challenge_response_keys) {
  if (challenge_response_keys.empty()) {
    // TODO(crbug.com/40568975): Indicate the error in the UI.
    if (pending_auth_state_) {
      std::move(pending_auth_state_->callback).Run(/*auth_success=*/false);
      pending_auth_state_.reset();
    }
    return;
  }

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  auto user_context = std::make_unique<UserContext>(*user);
  *user_context->GetMutableChallengeResponseKeys() =
      std::move(challenge_response_keys);
  ContinueAuthenticate(std::move(user_context));
}

void ScreenLocker::OnPinAttemptDone(std::unique_ptr<UserContext> user_context,
                                    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    // PIN authentication has failed; try submitting as a normal password.
    // Clear the label value so auth performer will look up the label for
    // the password factor.
    user_context->SetIsUsingPin(false);
    user_context->GetKey()->SetLabel("");
    ContinueAuthenticate(std::move(user_context));
    return;
  }

  OnAuthSuccess(std::move(*user_context));
}

void ScreenLocker::ContinueAuthenticate(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(!user_context->IsUsingPin());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ScreenLocker::AttemptUnlock, weak_factory_.GetWeakPtr(),
                     std::move(user_context)));
}

void ScreenLocker::AttemptUnlock(std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  // Retrieve accountId before std::move(user_context).
  const AccountId accountId = user_context->GetAccountId();

  authenticator_->AuthenticateToUnlock(
      user_manager::UserManager::Get()->IsEphemeralAccountId(accountId),
      std::move(user_context));
}

const user_manager::User* ScreenLocker::FindUnlockUser(
    const AccountId& account_id) {
  for (const user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      return user;
    }
  }
  return nullptr;
}

void ScreenLocker::OnStartLockCallback(bool locked) {
  // Happens in tests that exit with a pending lock. In real lock failure,
  // LockStateController would cause the current user session to be terminated.
  if (!locked) {
    return;
  }

  views_screen_locker_->OnAshLockAnimationFinished();
}

user_manager::UserList ScreenLocker::GetUsersToShow() const {
  user_manager::UserList users_to_show;
  // Filter out Managed Guest Session users as they should not appear on the UI.
  std::ranges::copy_if(users_, std::back_inserter(users_to_show),
                       [](const user_manager::User* user) {
                         return user->GetType() !=
                                user_manager::UserType::kPublicAccount;
                       });
  return users_to_show;
}

void ScreenLocker::ResetToLockedState() {
  CHECK(base::CurrentUIThread::IsSet());
  LoginScreen::Get()->GetModel()->ResetFingerprintUIState(
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  unlock_started_ = false;
}

void ScreenLocker::RefreshPinAndFingerprintTimeout() {
  MaybeDisablePinAndFingerprintFromTimeout(
      "RefreshPinAndFingerprintTimeout",
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
}

bool ScreenLocker::IsAuthTemporarilyDisabledForUser(
    const AccountId& account_id) {
  return users_with_temporarily_disabled_auth_.contains(account_id);
}

base::WeakAutoReset<ScreenLocker, scoped_refptr<Authenticator>>
ScreenLocker::SetAuthenticatorsForTesting(
    scoped_refptr<Authenticator> authenticator) {
  return base::WeakAutoReset<ScreenLocker, scoped_refptr<Authenticator>>(
      weak_factory_.GetWeakPtr(), &ScreenLocker::authenticator_,
      std::move(authenticator));
}

base::WeakAutoReset<ScreenLocker, raw_ptr<AuthStatusConsumer>>
ScreenLocker::SetLoginStatusConsumerForTesting(AuthStatusConsumer* consumer) {
  CHECK(!auth_status_consumer_for_testing_);
  return base::WeakAutoReset<ScreenLocker, raw_ptr<AuthStatusConsumer>>(
      weak_factory_.GetWeakPtr(),
      &ScreenLocker::auth_status_consumer_for_testing_, consumer);
}

// static
void ScreenLocker::SetClocksForTesting(const base::Clock* clock,
                                       const base::TickClock* tick_clock) {
  // Testing clocks should be already set at timer's initialization,
  // which happens in ScreenLocker's constructor.
  CHECK(base::CurrentUIThread::IsSet());
  CHECK(!ScreenLockerController::Get().screen_locker());
  g_clock_for_testing_ = clock;
  g_tick_clock_for_testing_ = tick_clock;
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::AuthState::AuthState(AccountId account_id,
                                   base::OnceCallback<void(bool)> callback)
    : account_id(account_id), callback(std::move(callback)) {}

ScreenLocker::AuthState::~AuthState() = default;

ScreenLocker::~ScreenLocker() {
  VLOG(1) << "Destroying ScreenLocker " << this;
  CHECK(base::CurrentUIThread::IsSet());
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);

  GetLoginScreenCertProviderService()
      ->pin_dialog_manager()
      ->RemovePinDialogHost(&security_token_pin_dialog_host_login_impl_);

  if (authenticator_) {
    authenticator_->SetConsumer(nullptr);
  }

  g_clock_for_testing_ = nullptr;
  g_tick_clock_for_testing_ = nullptr;

  VLOG(1) << "Calling session manager's HandleLockScreenDismissed D-Bus method";
  SessionManagerClient::Get()->NotifyLockScreenDismissed();

  if (saved_ime_state_.get()) {
    input_method::InputMethodManager::Get()->SetState(saved_ime_state_);
  }
}

void ScreenLocker::StartFingerprintAuthSession(
    const user_manager::User* primary_user) {
  auto* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForUser(primary_user);
  if (IsFingerprintAvailableForUser(quick_unlock_storage)) {
    VLOG(1) << "Fingerprint is available on lock screen.";
  } else {
    VLOG(1) << "Fingerprint is not available on lock screen.";
  }
  // Don't start a fingerprint auth session if the device does not have a
  // fingerprint sensor, or if the user does not have fingerprint records
  if (quick_unlock_storage->fingerprint_storage()->IsFingerprintAvailable(
          quick_unlock::Purpose::kUnlock)) {
    VLOG(1) << "Starting fingerprint AuthSession on the lock screen";
    fp_service_->StartAuthSession();
  }
}

void ScreenLocker::ScreenLockReady() {
  locked_ = true;
  base::TimeDelta delta = base::Time::Now() - start_time_;
  VLOG(1) << "ScreenLocker " << this << " is ready after " << delta.InSecondsF()
          << " second(s)";
  UMA_HISTOGRAM_TIMES("ScreenLocker.ScreenLockTime", delta);

  VLOG(1) << "Calling session manager's HandleLockScreenShown D-Bus method";
  SessionManagerClient::Get()->NotifyLockScreenShown();

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  StartFingerprintAuthSession(primary_user);

  // Update fingerprint state for the user once we get their record.
  // Note that we do not check if fingerprint is available for this user
  // because we want to catch an eventual late biod start (which would make us
  // believe that there is no record and a fortiori no fingerprint available).
  auto* profile = ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (profile) {
    fingerprint_pref_change_registrar_ =
        std::make_unique<PrefChangeRegistrar>();
    fingerprint_pref_change_registrar_->Init(profile->GetPrefs());
    fingerprint_pref_change_registrar_->Add(
        prefs::kQuickUnlockFingerprintRecord,
        base::BindRepeating(&ScreenLocker::UpdateFingerprintStateForUser,
                            weak_factory_.GetWeakPtr(), primary_user));
  }

  MaybeDisablePinAndFingerprintFromTimeout("ScreenLockReady",
                                           primary_user->GetAccountId());
}

bool ScreenLocker::IsUserLoggedIn(const AccountId& account_id) const {
  for (user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      return true;
    }
  }
  return false;
}

void ScreenLocker::OnRestarted() {}

void ScreenLocker::OnStatusChanged(
    device::mojom::BiometricsManagerStatus status) {
  switch (status) {
    case device::mojom::BiometricsManagerStatus::INITIALIZED:
      StartFingerprintAuthSession(
          user_manager::UserManager::Get()->GetPrimaryUser());
      return;
    case device::mojom::BiometricsManagerStatus::UNKNOWN:
    default:
      break;
  }
  NOTREACHED() << "ScreenLocker StatusChanged to an unknown state: "
               << static_cast<int>(status);
}

void ScreenLocker::OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                    bool enroll_session_complete,
                                    int percent_complete) {}

void ScreenLocker::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr msg,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  RefreshPinAndFingerprintTimeout();

  unlock_attempt_type_ = AUTH_FINGERPRINT;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  auto* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForUser(primary_user);
  if (!IsFingerprintAvailableForUser(quick_unlock_storage)) {
    // If fingerprint is not available for the primary user, exit early.
    quick_unlock_storage->fingerprint_storage()->RecordFingerprintUnlockResult(
        quick_unlock::FingerprintUnlockResult::kFingerprintUnavailable);
    return;
  }

  if (IsAuthTemporarilyDisabledForUser(primary_user->GetAccountId())) {
    quick_unlock_storage->fingerprint_storage()->RecordFingerprintUnlockResult(
        quick_unlock::FingerprintUnlockResult::kAuthTemporarilyDisabled);
    return;
  }

  LoginScreenClientImpl::Get()->auth_recorder()->RecordAuthMethod(
      LoginAuthRecorder::AuthMethod::kFingerprint);

  switch (msg->which()) {
    case device::mojom::FingerprintMessage::Tag::kScanResult:
      VLOG(1) << "Receive fingerprint auth scan result. scan_result="
              << msg->get_scan_result();
      if (msg->get_scan_result() != device::mojom::ScanResult::SUCCESS) {
        LOG(ERROR) << "Fingerprint unlock failed because scan_result="
                   << msg->get_scan_result();
        OnFingerprintAuthFailure(*primary_user);
        quick_unlock_storage->fingerprint_storage()
            ->RecordFingerprintUnlockResult(
                quick_unlock::FingerprintUnlockResult::kMatchFailed);
        return;
      }
      break;
    case device::mojom::FingerprintMessage::Tag::kFingerprintError:
      LOG(ERROR) << "Fingerprint unlock failed because error="
                 << msg->get_fingerprint_error() << " occurred.";
      OnFingerprintAuthFailure(*primary_user);
      quick_unlock_storage->fingerprint_storage()
          ->RecordFingerprintUnlockResult(
              quick_unlock::FingerprintUnlockResult::kMatchFailed);
      return;
  }

  UserContext user_context(*primary_user);
  if (!matches.contains(primary_user->username_hash())) {
    LOG(ERROR) << "Fingerprint unlock failed because it does not match primary"
               << " user's record";
    OnFingerprintAuthFailure(*primary_user);
    quick_unlock_storage->fingerprint_storage()->RecordFingerprintUnlockResult(
        quick_unlock::FingerprintUnlockResult::kMatchNotForPrimaryUser);
    return;
  }
  quick_unlock_storage->fingerprint_storage()->RecordFingerprintUnlockResult(
      quick_unlock::FingerprintUnlockResult::kSuccess);
  LoginScreen::Get()->GetModel()->NotifyFingerprintAuthResult(
      primary_user->GetAccountId(), /*success*/ true);
  fp_service_->EndCurrentAuthSession(base::BindOnce(
      &ScreenLocker::OnEndFingerprintAuthSession, weak_factory_.GetWeakPtr()));
  VLOG(1) << "Fingerprint unlock is successful.";
  OnAuthSuccess(user_context);
}

void ScreenLocker::OnSessionFailed() {
  LOG(ERROR) << "Fingerprint session failed.";
}

void ScreenLocker::ActiveUserChanged(user_manager::User* active_user) {
  // During ScreenLocker lifetime active user could only change when unlock has
  // started. See https://crbug.com/1022667 for more details.
  CHECK(unlock_started_);
}

void ScreenLocker::OnFingerprintAuthFailure(const user_manager::User& user) {
  UMA_HISTOGRAM_ENUMERATION("ScreenLocker.AuthenticationFailure",
                            unlock_attempt_type_, UnlockType::AUTH_COUNT);
  LoginScreen::Get()->GetModel()->NotifyFingerprintAuthResult(
      user.GetAccountId(), /*success*/ false);
  session_manager::SessionManager::Get()->NotifyUnlockAttempt(
      /*success*/ false, TransformUnlockType());

  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForUser(&user);
  if (quick_unlock_storage &&
      quick_unlock_storage->IsFingerprintAuthenticationAvailable(
          quick_unlock::Purpose::kUnlock)) {
    quick_unlock_storage->fingerprint_storage()->AddUnlockAttempt(
        base::TimeTicks::Now());
    if (quick_unlock_storage->fingerprint_storage()->ExceededUnlockAttempts()) {
      VLOG(1) << "Fingerprint unlock is disabled because it reached maximum"
              << " unlock attempt.";
      LoginScreen::Get()->GetModel()->SetFingerprintState(
          user.GetAccountId(), FingerprintState::DISABLED_FROM_ATTEMPTS);
    }
  }

  if (auth_status_consumer_for_testing_) {
    AuthFailure failure(AuthFailure::UNLOCK_FAILED);
    auth_status_consumer_for_testing_->OnAuthFailure(failure);
  }
}

void ScreenLocker::MaybeDisablePinAndFingerprintFromTimeout(
    const std::string& source,
    const AccountId& account_id) {
  VLOG(1) << "MaybeDisablePinAndFingerprintFromTimeout source=" << source;

  update_fingerprint_state_timer_->Stop();

  // Update PIN state.
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, quick_unlock::Purpose::kUnlock,
      base::BindOnce(&ScreenLocker::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), account_id));

  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  if (quick_unlock_storage) {
    if (quick_unlock_storage->HasStrongAuth()) {
      // Call this function again when strong authentication expires. PIN may
      // also depend on strong authentication if it is prefs-based. Fingerprint
      // always requires strong authentication.
      const base::Time next_strong_auth =
          quick_unlock_storage->TimeOfNextStrongAuth();
      VLOG(1) << "Scheduling next pin and fingerprint timeout check at "
              << next_strong_auth;
      update_fingerprint_state_timer_->Start(
          FROM_HERE, next_strong_auth,
          base::BindOnce(
              &ScreenLocker::MaybeDisablePinAndFingerprintFromTimeout,
              base::Unretained(this), "update_fingerprint_state_timer_",
              account_id));
    } else {
      // Strong auth is unavailable; update state to fingerprint disabled
      if (quick_unlock_storage->fingerprint_storage()->IsFingerprintAvailable(
              quick_unlock::Purpose::kUnlock)) {
        VLOG(1) << "Require strong auth to make fingerprint unlock available.";
        // End fingerprint auth session to stop biod from broadcasting
        // fingerprint scans.
        fp_service_->EndCurrentAuthSession(
            base::BindOnce(&ScreenLocker::OnEndFingerprintAuthSession,
                           weak_factory_.GetWeakPtr()));
        LoginScreen::Get()->GetModel()->SetFingerprintState(
            account_id, FingerprintState::DISABLED_FROM_TIMEOUT);
      }
    }
  }
}

void ScreenLocker::OnPinCanAuthenticate(
    const AccountId& account_id,
    bool can_authenticate,
    cryptohome::PinLockAvailability available_at) {
  LoginScreen::Get()->GetModel()->SetPinEnabledForUser(
      account_id, can_authenticate, available_at);
}

void ScreenLocker::UpdateFingerprintStateForUser(
    const user_manager::User* user) {
  LoginScreen::Get()->GetModel()->SetFingerprintState(
      user->GetAccountId(), quick_unlock::GetFingerprintStateForUser(
                                user, quick_unlock::Purpose::kUnlock));
}

session_manager::UnlockType ScreenLocker::TransformUnlockType() {
  session_manager::UnlockType unlock_type;
  switch (unlock_attempt_type_) {
    case AUTH_PASSWORD:
      unlock_type = session_manager::UnlockType::PASSWORD;
      break;
    case AUTH_PIN:
      unlock_type = session_manager::UnlockType::PIN;
      break;
    case AUTH_FINGERPRINT:
      unlock_type = session_manager::UnlockType::FINGERPRINT;
      break;
    case AUTH_CHALLENGE_RESPONSE:
      unlock_type = session_manager::UnlockType::CHALLENGE_RESPONSE;
      break;
    case AUTH_COUNT:
      unlock_type = session_manager::UnlockType::UNKNOWN;
      break;
  }
  return unlock_type;
}

void ScreenLocker::OnEndFingerprintAuthSession(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to end fingerprint auth session";
  }
}

}  // namespace ash
