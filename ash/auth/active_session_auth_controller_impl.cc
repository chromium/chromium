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
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
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
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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
  params.show_state = ui::SHOW_STATE_NORMAL;
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
  }
  NOTREACHED();
}

std::u16string BuildPinStatusMessage(const cryptohome::PinStatus& pin_status) {
  if (!pin_status.IsLockedFactor()) {
    return u"";
  }
  if (pin_status.AvailableAt() == base::Time::Max()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS);
  } else {
    base::TimeDelta delta = pin_status.AvailableAt() - base::Time::Now();
    std::u16string time_left_message;
    if (base::TimeDurationCompactFormatWithSeconds(
            delta, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
            &time_left_message)) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_IN_SESSION_AUTH_PIN_DELAY_REQUIRED, time_left_message);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS);
    }
  }
}

}  // namespace

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

void ActiveSessionAuthControllerImpl::TestApi::DisplayPinStatusMessage(
    const cryptohome::PinStatus pin_status) {
  controller_->DisplayPinStatusMessage(pin_status);
}

const std::u16string&
ActiveSessionAuthControllerImpl::TestApi::GetPinStatusMessage() const {
  return controller_->pin_status_message_;
}

void ActiveSessionAuthControllerImpl::TestApi::Close() {
  controller_->Close();
}

ActiveSessionAuthControllerImpl::ActiveSessionAuthControllerImpl() = default;
ActiveSessionAuthControllerImpl::~ActiveSessionAuthControllerImpl() = default;

bool ActiveSessionAuthControllerImpl::ShowAuthDialog(
    std::unique_ptr<AuthRequest> auth_request) {
  CHECK(auth_request);
  LOG(WARNING) << "Show is requested with reason: "
               << ReasonToString(auth_request->GetAuthReason());
  if (IsShown()) {
    LOG(ERROR) << "ActiveSessionAuthController widget is already exists.";
    auth_request->NotifyAuthFailure();
    return false;
  }

  CHECK(!auth_request_);
  auth_request_ = std::move(auth_request);

  title_ = l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE);
  description_ = l10n_util::GetStringUTF16(auth_request_->GetDescription());
  auth_factor_editor_ =
      std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get());
  auth_performer_ = std::make_unique<AuthPerformer>(UserDataAuthClient::Get());
  account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();

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

void ActiveSessionAuthControllerImpl::OnAuthSessionStarted(
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (!user_exists || authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << authentication_error->get_cryptohome_code();
    Close();
    return;
  }

  uma_recorder_.RecordShow(auth_request_->GetAuthReason());
  UserDataAuthClient::Get()->AddAuthFactorStatusUpdateObserver(this);

  available_factors_.Clear();
  user_context_ = std::move(user_context);
  const auto& auth_factors = user_context_->GetAuthFactorsData();

  if (auth_factors.FindAnyPasswordFactor()) {
    available_factors_.Put(AuthInputType::kPassword);
  }

  if (auth_factors.FindPinFactor() && !IsPinLocked()) {
    available_factors_.Put(AuthInputType::kPin);
  }

  InitUi();
}

void ActiveSessionAuthControllerImpl::InitUi() {
  auto contents_view = std::make_unique<ActiveSessionAuthView>(
      account_id_, title_, description_, available_factors_);
  contents_view_ = contents_view.get();

  widget_ = CreateAuthDialogWidget(std::move(contents_view));
  contents_view_observer_.Observe(contents_view_);
  contents_view_->AddObserver(this);
  SetState(ActiveSessionAuthState::kInitialized);

  MoveToTheCenter();
  widget_->Show();
  ash::AuthParts::Get()
      ->GetAuthSurfaceRegistry()
      ->NotifyInSessionAuthDialogShown();
}

void ActiveSessionAuthControllerImpl::Close() {
  LOG(WARNING) << "Close with : " << ActiveSessionAuthStateToString(state_)
               << " state.";
  uma_recorder_.RecordClose();
  contents_view_observer_.Reset();
  CHECK(contents_view_);
  contents_view_->RemoveObserver(this);
  contents_view_ = nullptr;
  SetState(ActiveSessionAuthState::kWaitForInit);

  pending_pin_factor_status_update_.reset();
  UserDataAuthClient::Get()->RemoveAuthFactorStatusUpdateObserver(this);
  if (auth_performer_) {
    auth_performer_->InvalidateCurrentAttempts();
    auth_performer_.reset();
  }
  auth_factor_editor_.reset();

  title_.clear();
  description_.clear();

  widget_.reset();

  if (auth_request_) {
    auth_request_->NotifyAuthFailure();
    auth_request_.reset();
  }

  if (user_context_) {
    user_context_.reset();
  }

  available_factors_.Clear();
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
  if (authentication_error.has_value()) {
    uma_recorder_.RecordAuthFailed(input_type);
    user_context_ = std::move(user_context);
    if (pending_pin_factor_status_update_.has_value()) {
      ProcessAuthFactorStatusUpdate(pending_pin_factor_status_update_.value());
      pending_pin_factor_status_update_.reset();
    }
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
    auth_request_->NotifyAuthSuccess(std::move(user_context));
    auth_request_.reset();
    Close();
  }
}

void ActiveSessionAuthControllerImpl::OnClose() {
  Close();
}

bool ActiveSessionAuthControllerImpl::IsPinLocked() const {
  CHECK(user_context_);
  const auto& auth_factors = user_context_->GetAuthFactorsData();
  auto* pin_factor = auth_factors.FindPinFactor();
  CHECK(pin_factor);
  return pin_factor->GetPinStatus().IsLockedFactor();
}

void ActiveSessionAuthControllerImpl::SetState(ActiveSessionAuthState state) {
  LOG(WARNING) << "SetState is requested from: "
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
  }
  state_ = state;
}

void ActiveSessionAuthControllerImpl::OnAuthFactorStatusUpdate(
    const user_data_auth::AuthFactorStatusUpdate& update) {
  if (update.auth_factor_with_status().auth_factor().type() ==
      user_data_auth::AUTH_FACTOR_TYPE_PIN) {
    if (user_context_) {
      ProcessAuthFactorStatusUpdate(update);
    } else {
      if (pending_pin_factor_status_update_.has_value()) {
        LOG(WARNING) << "Overwrite pending pin status update.";
      }
      pending_pin_factor_status_update_ = update;
    }
  }
}

void ActiveSessionAuthControllerImpl::ProcessAuthFactorStatusUpdate(
    const user_data_auth::AuthFactorStatusUpdate& update) {
  CHECK(user_context_);
  // Broadcast id is a public id of an ongoing auth session.
  // Generally it is unlikely to have two active auth session running
  // in parallel, but we need to make sure we only react to signals
  // mapped to this session.
  if (user_context_->GetBroadcastId() == update.broadcast_id()) {
    auto auth_factor = cryptohome::DeserializeAuthFactor(
        update.auth_factor_with_status(),
        /*fallback_type=*/cryptohome::AuthFactorType::kPassword);
    if (auth_factor.ref().type() == cryptohome::AuthFactorType::kPin) {
      CHECK(contents_view_);
      auto pin_status = auth_factor.GetPinStatus();
      bool pin_enabled = !pin_status.IsLockedFactor();
      // Only need to update the auth view because |available_factors_| is only
      // used to initialize the view.
      contents_view_->SetHasPin(pin_enabled);
      DisplayPinStatusMessage(pin_status);
    }
  }
}

void ActiveSessionAuthControllerImpl::DisplayPinStatusMessage(
    const cryptohome::PinStatus pin_status) {
  pin_status_message_ = BuildPinStatusMessage(pin_status);
  contents_view_->SetPinStatus(pin_status_message_);
}

}  // namespace ash
