// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/active_session_auth_controller_impl.h"

#include <memory>
#include <string>

#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/auth_surface_registry.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

// Read the salt from local state.
std::string GetUserSalt(const AccountId& account_id) {
  user_manager::KnownUser known_user(Shell::Get()->local_state());
  if (const std::string* salt =
          known_user.FindStringPath(account_id, prefs::kQuickUnlockPinSalt)) {
    return *salt;
  }
  return {};
}

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::mojom::WindowShowState::kNormal;
  CHECK_EQ(Shell::Get()->session_controller()->GetSessionState(),
           session_manager::SessionState::ACTIVE);
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_SystemModalContainer);
  params.autosize = true;
  params.name = "AuthDialogWidget";

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::mojom::ModalType::kSystem);
  params.delegate->SetOwnedByWidget(true);

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

const char* ReasonToString(AuthRequest::Reason reason) {
  switch (reason) {
    case AuthRequest::Reason::kPasswordManager:
      return "PasswordManager";
    case AuthRequest::Reason::kSettings:
      return "Settings";
    case AuthRequest::Reason::kWebAuthN:
      return "WebAuthN";
  }
  NOTREACHED();
}

const char* ActiveSessionAuthStateToString(
    ActiveSessionAuthControllerImpl::ActiveSessionAuthState state) {
  switch (state) {
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::kWaitForInit:
      return "WaitForInit";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::kInitialized:
      return "Initialized";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kPasswordAuthStarted:
      return "PasswordAuthStarted";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kPasswordAuthSucceeded:
      return "PasswordAuthSucceeded";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kPinAuthStarted:
      return "PinAuthStarted";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kPinAuthSucceeded:
      return "PinAuthSucceeded";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kFingerprintAuthSucceeded:
      return "FingerprintAuthSucceeded";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kFingerprintAuthSucceededWaiting:
      return "FingerprintAuthSucceededWaiting";
    case ActiveSessionAuthControllerImpl::ActiveSessionAuthState::
        kCloseRequested:
      return "CloseRequested";
  }
  NOTREACHED();
}

}  // namespace

// This class manages the closing process after successful fingerprint
// authentication. It listens for two signals:
//  1. The completion of the successful authentication animation.
//  2. The authentication callback from cryptohome.
// Once both signals are received, the class triggers the closing process.
class ActiveSessionAuthControllerImpl::FingerprintAuthTracker {
 public:
  explicit FingerprintAuthTracker(ActiveSessionAuthControllerImpl* owner)
      : owner_(owner) {
    CHECK(owner_);
  }

  void OnAuthenticationFinished(
      std::unique_ptr<UserContext> user_context,
      std::optional<AuthenticationError> authentication_error) {
    CHECK_EQ(authentication_finished_, false);
    authentication_finished_ = true;
    if (authentication_error.has_value()) {
      LOG(ERROR) << "Authentication error during OnFingerprintSuccess code: "
                 << authentication_error->get_cryptohome_code();
    }
    owner_->user_context_ = std::move(user_context);
    MaybeNotifyOwner();
  }

  void OnAnimationFinished() {
    VLOG(1) << "OnAnimationFinished";
    CHECK_EQ(animation_finished_, false);
    animation_finished_ = true;
    MaybeNotifyOwner();
  }

  void MaybeNotifyOwner() {
    if (authentication_finished_ && animation_finished_) {
      owner_->StartClose();
    }
    CHECK(owner_);
    CHECK(owner_->fp_auth_tracker_);
  }

 private:
  const raw_ptr<ActiveSessionAuthControllerImpl> owner_;
  bool animation_finished_ = false;
  bool authentication_finished_ = false;
};

ActiveSessionAuthControllerImpl::TestApi::TestApi(
    ActiveSessionAuthControllerImpl* controller)
    : controller_(controller) {}

ActiveSessionAuthControllerImpl::TestApi::~TestApi() = default;

AuthFactorSet ActiveSessionAuthControllerImpl::TestApi::GetAvailableFactors()
    const {
  return controller_->available_factors_;
}

void ActiveSessionAuthControllerImpl::TestApi::SubmitPassword(
    const std::string& password) {
  controller_->OnPasswordSubmit(base::UTF8ToUTF16(password));
}

void ActiveSessionAuthControllerImpl::TestApi::SubmitPin(
    const std::string& pin) {
  controller_->OnPinSubmit(base::UTF8ToUTF16(pin));
}

void ActiveSessionAuthControllerImpl::TestApi::Close() {
  controller_->StartClose();
}

void ActiveSessionAuthControllerImpl::TestApi::SetPinStatus(
    std::unique_ptr<cryptohome::PinStatus> pin_status) {
  controller_->contents_view_->SetPinStatus(std::move(pin_status));
}

const std::u16string&
ActiveSessionAuthControllerImpl::TestApi::GetPinStatusMessage() const {
  return controller_->contents_view_->GetPinStatusMessage();
}

ActiveSessionAuthControllerImpl::ActiveSessionAuthControllerImpl() = default;
ActiveSessionAuthControllerImpl::~ActiveSessionAuthControllerImpl() = default;

bool ActiveSessionAuthControllerImpl::IsSucceedState() const {
  return state_ == ActiveSessionAuthState::kPasswordAuthSucceeded ||
         state_ == ActiveSessionAuthState::kPinAuthSucceeded ||
         state_ == ActiveSessionAuthState::kFingerprintAuthSucceeded ||
         state_ == ActiveSessionAuthState::kFingerprintAuthSucceededWaiting;
}

bool ActiveSessionAuthControllerImpl::ShowAuthDialog(
    std::unique_ptr<AuthRequest> auth_request) {
  CHECK(auth_request);
  VLOG(1) << "Show is requested with reason: "
          << ReasonToString(auth_request->GetAuthReason());
  if (IsShown()) {
    LOG(ERROR) << "ActiveSessionAuthController widget is already exists.";
    auth_request->NotifyAuthFailure();
    return false;
  }

  CHECK(!auth_request_);
  auth_request_ = std::move(auth_request);

  title_ = l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE);
  description_ = auth_request_->GetDescription();
  auth_factor_editor_ =
      std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get());
  auth_performer_ = std::make_unique<AuthPerformer>(UserDataAuthClient::Get());
  account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();

  fingerprint_animation_finished_ = false;
  fingerprint_authentication_finished_ = false;

  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  auto user_context = std::make_unique<UserContext>(*active_user);

  const bool ephemeral =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          account_id_);

  auth_performer_->StartAuthSession(
      std::move(user_context), ephemeral, auth_request_->GetAuthSessionIntent(),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthSessionStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  return true;
}

bool ActiveSessionAuthControllerImpl::IsShown() const {
  return widget_ != nullptr;
}

void ActiveSessionAuthControllerImpl::SetFingerprintClient(
    ActiveSessionFingerprintClient* fp_client) {
  CHECK_NE(fp_client_ == nullptr, fp_client == nullptr);
  fp_client_ = fp_client;
}

void ActiveSessionAuthControllerImpl::OnAuthSessionStarted(
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  user_context_ = std::move(user_context);

  if (!user_exists || authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << authentication_error->get_cryptohome_code();
    StartClose();
    return;
  }

  auth_session_broadcast_id_ = user_context_->GetBroadcastId();

  uma_recorder_.RecordShow(auth_request_->GetAuthReason());
  UserDataAuthClient::Get()->AddAuthFactorStatusUpdateObserver(this);

  available_factors_.Clear();
  const auto& auth_factors = user_context_->GetAuthFactorsData();

  if (auth_factors.FindAnyPasswordFactor()) {
    available_factors_.Put(AuthInputType::kPassword);
  }

  auto* pin_factor = auth_factors.FindPinFactor();
  if (pin_factor) {
    if (!pin_factor->GetPinStatus().IsLockedFactor()) {
      available_factors_.Put(AuthInputType::kPin);
    }
  }

  MaybePrepareFingerprint(
      BindOnce(&ActiveSessionAuthControllerImpl::AuthFactorsAreReady,
               weak_ptr_factory_.GetWeakPtr()));
}

void ActiveSessionAuthControllerImpl::MaybePrepareFingerprint(
    AuthFactorsReadyCallback on_auth_factors_ready) {
  // If the fingerprint factor is allowed to use by the user for the current
  // reason then start it before initialize the UI.
  if (fp_client_ && fp_client_->IsFingerprintAvailable(
                        auth_request_->GetAuthReason(), account_id_)) {
    VLOG(1) << "PrepareFingerprintAuth started.";
    fp_client_->PrepareFingerprintAuth(
        std::move(user_context_),
        /* auth_ready_callback = */
        base::BindOnce(&ActiveSessionAuthControllerImpl::OnFingerprintReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_auth_factors_ready)),
        /* on_scan_callback = */
        base::BindRepeating(&ActiveSessionAuthControllerImpl::OnFingerprintScan,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  std::move(on_auth_factors_ready).Run(std::move(user_context_));
}

void ActiveSessionAuthControllerImpl::OnFingerprintReady(
    AuthFactorsReadyCallback on_auth_factors_ready,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start fingerprint auth session - only "
                  "non-fingerprint factors will be available.";
  } else {
    fp_auth_tracker_ = std::make_unique<FingerprintAuthTracker>(this);
    available_factors_.Put(AuthInputType::kFingerprint);
  }
  std::move(on_auth_factors_ready).Run(std::move(user_context));
}

void ActiveSessionAuthControllerImpl::AuthFactorsAreReady(
    std::unique_ptr<UserContext> user_context) {
  user_context_ = std::move(user_context);
  InitUi();
}

void ActiveSessionAuthControllerImpl::OnFingerprintScan(
    const FingerprintAuthScanResult scan_result) {
  CHECK_NE(state_, ActiveSessionAuthState::kWaitForInit);
  // Avoid unnecessary processing if we've already initiated close.
  if (IsSucceedState() || state_ == ActiveSessionAuthState::kCloseRequested) {
    return;
  }
  switch (scan_result) {
    case FingerprintAuthScanResult::kSuccess:
      contents_view_->NotifyFingerprintAuthSuccess(
          base::BindOnce(&FingerprintAuthTracker::OnAnimationFinished,
                         base::Unretained(fp_auth_tracker_.get())));
      if (state_ == ActiveSessionAuthState::kPasswordAuthStarted ||
          state_ == ActiveSessionAuthState::kPinAuthStarted) {
        SetState(ActiveSessionAuthState::kFingerprintAuthSucceededWaiting);
        // The user_context_ is not available, we have to wait for the
        // OnAuthComplete callback to have UserContext.
        return;
      }
      HandleFingerprintAuthSuccess();
      return;
    case FingerprintAuthScanResult::kTooManyAttempts:
      uma_recorder_.RecordAuthFailed(AuthInputType::kFingerprint);

      contents_view_->SetFingerprintState(
          FingerprintState::DISABLED_FROM_ATTEMPTS);
      return;
    case FingerprintAuthScanResult::kFailed:
      uma_recorder_.RecordAuthFailed(AuthInputType::kFingerprint);

      contents_view_->NotifyFingerprintAuthFailure();
      return;
    case FingerprintAuthScanResult::kFatalError:
      contents_view_->SetFingerprintState(FingerprintState::UNAVAILABLE);
      return;
  }
  NOTREACHED();
}

void ActiveSessionAuthControllerImpl::HandleFingerprintAuthSuccess() {
  CHECK(user_context_);
  uma_recorder_.RecordAuthSucceeded(AuthInputType::kFingerprint);
  SetState(ActiveSessionAuthState::kFingerprintAuthSucceeded);
  auth_performer_->AuthenticateWithLegacyFingerprint(
      std::move(user_context_),
      base::BindOnce(&FingerprintAuthTracker::OnAuthenticationFinished,
                     base::Unretained(fp_auth_tracker_.get())));
}

void ActiveSessionAuthControllerImpl::InitUi() {
  auto contents_view = std::make_unique<ActiveSessionAuthView>(
      account_id_, title_, description_, available_factors_);
  contents_view_ = contents_view.get();

  widget_ = CreateAuthDialogWidget(std::move(contents_view));
  contents_view_observer_.Observe(contents_view_);
  contents_view_->AddObserver(this);
  SetState(ActiveSessionAuthState::kInitialized);

  const auto& auth_factors = user_context_->GetAuthFactorsData();

  auto* pin_factor = auth_factors.FindPinFactor();
  if (pin_factor) {
    contents_view_->SetPinStatus(
        std::make_unique<cryptohome::PinStatus>(pin_factor->GetPinStatus()));
  }

  MoveToTheCenter();
  widget_->Show();
  ash::AuthParts::Get()
      ->GetAuthSurfaceRegistry()
      ->NotifyInSessionAuthDialogShown();
}

void ActiveSessionAuthControllerImpl::StartClose() {
  VLOG(1) << "Close with : " << ActiveSessionAuthStateToString(state_)
          << " state.";

  CHECK(user_context_);
  CHECK(auth_request_);
  CHECK(auth_performer_);
  if (state_ != ActiveSessionAuthState::kWaitForInit) {
    uma_recorder_.RecordClose();
  }
  contents_view_observer_.Reset();
  CHECK(contents_view_);
  contents_view_->RemoveObserver(this);
  contents_view_ = nullptr;
  auth_session_broadcast_id_.clear();

  UserDataAuthClient::Get()->RemoveAuthFactorStatusUpdateObserver(this);

  auth_performer_->InvalidateCurrentAttempts();
  if (fp_client_ && available_factors_.Has(AuthInputType::kFingerprint)) {
    CHECK(fp_auth_tracker_);
    fp_client_->TerminateFingerprintAuth(
        std::move(user_context_),
        base::BindOnce(&ActiveSessionAuthControllerImpl::CompleteClose,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  CHECK(!fp_auth_tracker_);
  CompleteClose(std::move(user_context_), std::nullopt);
}

void ActiveSessionAuthControllerImpl::CompleteClose(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  user_context_ = std::move(user_context);
  CHECK(user_context_);
  CHECK(auth_request_);
  auth_performer_.reset();
  auth_factor_editor_.reset();

  if (IsSucceedState()) {
    auth_request_->NotifyAuthSuccess(std::move(user_context_));
  } else {
    auth_request_->NotifyAuthFailure();
    user_context_.reset();
  }
  auth_request_.reset();
  available_factors_.Clear();

  SetState(ActiveSessionAuthState::kWaitForInit);

  title_.clear();
  description_.clear();
  fp_auth_tracker_.reset();
  widget_.reset();
}

void ActiveSessionAuthControllerImpl::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  MoveToTheCenter();
}

void ActiveSessionAuthControllerImpl::MoveToTheCenter() {
  widget_->CenterWindow(widget_->GetContentsView()->GetPreferredSize());
}

void ActiveSessionAuthControllerImpl::OnPasswordSubmit(
    const std::u16string& password) {
  if (IsSucceedState()) {
    return;
  }
  SetState(ActiveSessionAuthState::kPasswordAuthStarted);
  uma_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  CHECK(user_context_);
  const auto* password_factor =
      user_context_->GetAuthFactorsData().FindAnyPasswordFactor();
  CHECK(password_factor);

  const cryptohome::KeyLabel key_label = password_factor->ref().label();

  auth_performer_->AuthenticateWithPassword(
      key_label.value(), base::UTF16ToUTF8(password), std::move(user_context_),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPassword));
}

void ActiveSessionAuthControllerImpl::OnPinSubmit(const std::u16string& pin) {
  if (IsSucceedState()) {
    return;
  }
  SetState(ActiveSessionAuthState::kPinAuthStarted);
  uma_recorder_.RecordAuthStarted(AuthInputType::kPin);
  CHECK(user_context_);
  user_manager::KnownUser known_user(Shell::Get()->local_state());
  const std::string salt = GetUserSalt(account_id_);

  auth_performer_->AuthenticateWithPin(
      base::UTF16ToUTF8(pin), salt, std::move(user_context_),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPin));
}

void ActiveSessionAuthControllerImpl::OnAuthComplete(
    AuthInputType input_type,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  user_context_ = std::move(user_context);
  // If fingerprint auth succeeded during wait for PIN/password authentication,
  // handle success directly.
  if (state_ == ActiveSessionAuthState::kFingerprintAuthSucceededWaiting) {
    HandleFingerprintAuthSuccess();
    return;
  }
  CHECK(!IsSucceedState());
  if (state_ == ActiveSessionAuthState::kCloseRequested) {
    StartClose();
    return;
  }
  if (authentication_error.has_value()) {
    uma_recorder_.RecordAuthFailed(input_type);
    contents_view_->SetErrorTitle(l10n_util::GetStringUTF16(
        input_type == AuthInputType::kPassword
            ? IDS_ASH_IN_SESSION_AUTH_PASSWORD_INCORRECT
            : IDS_ASH_IN_SESSION_AUTH_PIN_INCORRECT));
    SetState(ActiveSessionAuthState::kInitialized);
  } else {
    uma_recorder_.RecordAuthSucceeded(input_type);
    SetState(input_type == AuthInputType::kPassword
                 ? ActiveSessionAuthState::kPasswordAuthSucceeded
                 : ActiveSessionAuthState::kPinAuthSucceeded);
    StartClose();
  }
}

void ActiveSessionAuthControllerImpl::OnClose() {
  switch (state_) {
    case ActiveSessionAuthState::kWaitForInit:
      NOTREACHED();
    case ActiveSessionAuthState::kInitialized:
      StartClose();
      return;
    case ActiveSessionAuthState::kPasswordAuthStarted:
    case ActiveSessionAuthState::kPinAuthStarted:
      SetState(ActiveSessionAuthState::kCloseRequested);
      return;
    case ActiveSessionAuthState::kPasswordAuthSucceeded:
    case ActiveSessionAuthState::kPinAuthSucceeded:
    case ActiveSessionAuthState::kFingerprintAuthSucceeded:
    case ActiveSessionAuthState::kFingerprintAuthSucceededWaiting:
    case ActiveSessionAuthState::kCloseRequested:
      return;
  }
  NOTREACHED();
}

void ActiveSessionAuthControllerImpl::SetState(ActiveSessionAuthState state) {
  VLOG(1) << "SetState is requested from: "
          << ActiveSessionAuthStateToString(state_)
          << " state to : " << ActiveSessionAuthStateToString(state)
          << " state.";
  switch (state) {
    case ActiveSessionAuthState::kWaitForInit:
      break;
    case ActiveSessionAuthState::kInitialized:
      CHECK(state_ == ActiveSessionAuthState::kWaitForInit ||
            state_ == ActiveSessionAuthState::kPasswordAuthStarted ||
            state_ == ActiveSessionAuthState::kPinAuthStarted);
      contents_view_->SetInputEnabled(true);
      break;
    case ActiveSessionAuthState::kPasswordAuthStarted:
      // Disable the UI while we are waiting for the response, except the close
      // button.
      CHECK_EQ(state_, ActiveSessionAuthState::kInitialized);
      contents_view_->SetInputEnabled(false);
      break;
    case ActiveSessionAuthState::kPasswordAuthSucceeded:
      CHECK_EQ(state_, ActiveSessionAuthState::kPasswordAuthStarted);
      break;
    case ActiveSessionAuthState::kPinAuthStarted:
      CHECK_EQ(state_, ActiveSessionAuthState::kInitialized);
      contents_view_->SetInputEnabled(false);
      break;
    case ActiveSessionAuthState::kPinAuthSucceeded:
      CHECK_EQ(state_, ActiveSessionAuthState::kPinAuthStarted);
      break;
    case ActiveSessionAuthState::kFingerprintAuthSucceeded:
      CHECK(state_ == ActiveSessionAuthState::kInitialized ||
            state_ == ActiveSessionAuthState::kFingerprintAuthSucceededWaiting);
      contents_view_->SetInputEnabled(false);
      break;
    case ActiveSessionAuthState::kFingerprintAuthSucceededWaiting:
      CHECK(state_ == ActiveSessionAuthState::kPasswordAuthStarted ||
            state_ == ActiveSessionAuthState::kPinAuthStarted);
      contents_view_->SetInputEnabled(false);
      break;
    case ActiveSessionAuthState::kCloseRequested:
      CHECK(state_ == ActiveSessionAuthState::kPasswordAuthStarted ||
            state_ == ActiveSessionAuthState::kPinAuthStarted);
      contents_view_->SetInputEnabled(false);
      break;
  }
  state_ = state;
}

void ActiveSessionAuthControllerImpl::OnAuthFactorStatusUpdate(
    const user_data_auth::AuthFactorStatusUpdate& update) {
  switch (state_) {
    case ActiveSessionAuthState::kInitialized:
    case ActiveSessionAuthState::kPinAuthStarted:
    case ActiveSessionAuthState::kPasswordAuthStarted:
      CHECK_NE(auth_session_broadcast_id_, "");
      if (auth_session_broadcast_id_ == update.broadcast_id()) {
        auto auth_factor = cryptohome::DeserializeAuthFactor(
            update.auth_factor_with_status(),
            /*fallback_type=*/cryptohome::AuthFactorType::kPassword);
        if (auth_factor.ref().type() == cryptohome::AuthFactorType::kPin) {
          auto pin_status = auth_factor.GetPinStatus();
          contents_view_->SetPinStatus(
              std::make_unique<cryptohome::PinStatus>(pin_status));
        }
      }
      return;

    case ActiveSessionAuthState::kWaitForInit:
      return;

    case ActiveSessionAuthState::kPasswordAuthSucceeded:
    case ActiveSessionAuthState::kPinAuthSucceeded:
    case ActiveSessionAuthState::kFingerprintAuthSucceeded:
    case ActiveSessionAuthState::kFingerprintAuthSucceededWaiting:
    case ActiveSessionAuthState::kCloseRequested:
      // No need to handle PIN updates as dialog closing is in progress.
      return;
  }
  NOTREACHED();
}

}  // namespace ash
