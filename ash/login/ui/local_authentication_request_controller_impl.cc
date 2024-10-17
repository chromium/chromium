// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_controller_impl.h"

#include <string>
#include <utility>

#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
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
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

LocalAuthenticationRequestControllerImpl::
    LocalAuthenticationRequestControllerImpl() = default;

LocalAuthenticationRequestControllerImpl::
    ~LocalAuthenticationRequestControllerImpl() = default;

void LocalAuthenticationRequestControllerImpl::OnClose() {}

bool LocalAuthenticationRequestControllerImpl::ShowWidget(
    LocalAuthenticationCallback local_authentication_callback,
    std::unique_ptr<UserContext> user_context) {
  if (LocalAuthenticationRequestWidget::Get()) {
    LOG(ERROR) << "LocalAuthenticationRequestWidget is already shown.";
    return false;
  }

  const auto& auth_factors = user_context->GetAuthFactorsData();
  const cryptohome::AuthFactor* local_password_factor =
      auth_factors.FindLocalPasswordFactor();
  if (local_password_factor == nullptr) {
    LOG(ERROR) << "The local password authentication factor is not available, "
                  "skip to show the local authentication dialog.";
    // TODO(b/334215182): It seems sometimes this dialog appears even when the
    // local password is not available.
    base::debug::DumpWithoutCrashing();
    return false;
  }

  const AccountId& account_id = user_context->GetAccountId();

  const std::string& user_email = account_id.GetUserEmail();

  const std::u16string desc = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_DESCRIPTION,
      base::UTF8ToUTF16(user_email));

  LocalAuthenticationRequestWidget::Show(
      std::move(local_authentication_callback),
      l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_TITLE),
      desc, weak_factory_.GetWeakPtr(), std::move(user_context));
  return true;
}

bool LocalAuthenticationRequestControllerImpl::IsDialogVisible() const {
  return LocalAuthenticationRequestWidget::Get() != nullptr;
}

bool LocalAuthenticationRequestControllerImpl::IsPinSupported() const {
  return false;
}

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

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? kShellWindowId_SystemModalContainer
          : kShellWindowId_LockSystemModalContainer;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);

  params.autosize = true;
  params.name = "AuthDialogWidget";

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  //  ModalType::kSystem is used to get a semi-transparent background behind the
  //  local authentication request view, when it is used directly on a widget.
  //  The overlay consumes all the inputs from the user, so that they can only
  //  interact with the local authentication request view while it is visible.
  params.delegate->SetModalType(ui::mojom::ModalType::kSystem);
  params.delegate->SetOwnedByWidget(true);

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

const char* LocalAuthenticationWithPinStateToString(
    LocalAuthenticationWithPinControllerImpl::LocalAuthenticationWithPinState
        state) {
  switch (state) {
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kWaitForInit:
      return "WaitForInit";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kInitialized:
      return "Initialized";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kPasswordAuthStarted:
      return "PasswordAuthStarted";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kPasswordAuthSucceeded:
      return "PasswordAuthSucceeded";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kPinAuthStarted:
      return "PinAuthStarted";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kPinAuthSucceeded:
      return "PinAuthSucceeded";
    case LocalAuthenticationWithPinControllerImpl::
        LocalAuthenticationWithPinState::kCloseRequested:
      return "CloseRequested";
  }
  NOTREACHED();
}

}  // namespace

LocalAuthenticationWithPinControllerImpl::
    LocalAuthenticationWithPinControllerImpl() = default;
LocalAuthenticationWithPinControllerImpl::
    ~LocalAuthenticationWithPinControllerImpl() = default;

bool LocalAuthenticationWithPinControllerImpl::IsSucceedState() const {
  return state_ == LocalAuthenticationWithPinState::kPasswordAuthSucceeded ||
         state_ == LocalAuthenticationWithPinState::kPinAuthSucceeded;
}

bool LocalAuthenticationWithPinControllerImpl::ShowWidget(
    LocalAuthenticationCallback local_authentication_callback,
    std::unique_ptr<UserContext> user_context) {
  CHECK(user_context);

  if (widget_ != nullptr) {
    LOG(ERROR) << "LocalAuthenticationWithPinController widget already exists.";
    std::move(local_authentication_callback)
        .Run(false, std::move(user_context));
    return false;
  }

  title_ = l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_TITLE);

  account_id_ = user_context->GetAccountId();

  description_ = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_DESCRIPTION,
      base::UTF8ToUTF16(account_id_.GetUserEmail()));

  auth_performer_ = std::make_unique<AuthPerformer>(UserDataAuthClient::Get());

  user_context_ = std::move(user_context);
  local_authentication_callback_ = std::move(local_authentication_callback);
  auth_session_broadcast_id_ = user_context_->GetBroadcastId();

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

  auto contents_view = std::make_unique<ActiveSessionAuthView>(
      account_id_, title_, description_, available_factors_);
  contents_view_ = contents_view.get();

  widget_ = CreateAuthDialogWidget(std::move(contents_view));
  contents_view_observer_.Observe(contents_view_);
  contents_view_->AddObserver(this);
  SetState(LocalAuthenticationWithPinState::kInitialized);

  if (pin_factor) {
    contents_view_->SetPinStatus(
        std::make_unique<cryptohome::PinStatus>(pin_factor->GetPinStatus()));
  }

  MoveToTheCenter();
  widget_->Show();

  return true;
}

bool LocalAuthenticationWithPinControllerImpl::IsDialogVisible() const {
  return state_ != LocalAuthenticationWithPinState::kWaitForInit;
}

bool LocalAuthenticationWithPinControllerImpl::IsPinSupported() const {
  return true;
}

void LocalAuthenticationWithPinControllerImpl::StartClose() {
  VLOG(1) << "Close with : " << LocalAuthenticationWithPinStateToString(state_)
          << " state.";

  CHECK(user_context_);
  CHECK(auth_performer_);
  contents_view_observer_.Reset();
  CHECK(contents_view_);
  contents_view_->RemoveObserver(this);
  contents_view_ = nullptr;
  auth_session_broadcast_id_.clear();

  UserDataAuthClient::Get()->RemoveAuthFactorStatusUpdateObserver(this);

  auth_performer_->InvalidateCurrentAttempts();
  auth_performer_.reset();

  available_factors_.Clear();

  bool success = IsSucceedState();

  SetState(LocalAuthenticationWithPinState::kWaitForInit);

  title_.clear();
  description_.clear();
  widget_.reset();

  std::move(local_authentication_callback_)
      .Run(success, std::move(user_context_));
}

void LocalAuthenticationWithPinControllerImpl::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  MoveToTheCenter();
}

void LocalAuthenticationWithPinControllerImpl::MoveToTheCenter() {
  widget_->CenterWindow(widget_->GetContentsView()->GetPreferredSize());
}

void LocalAuthenticationWithPinControllerImpl::OnPasswordSubmit(
    const std::u16string& password) {
  if (IsSucceedState()) {
    return;
  }

  CHECK_EQ(state_, LocalAuthenticationWithPinState::kInitialized);

  std::string password_utf8;
  bool conversion_succeeded =
      base::UTF16ToUTF8(password.data(), password.length(), &password_utf8);

  if (!conversion_succeeded) {
    contents_view_->SetErrorTitle(
        l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PASSWORD_INVALID));
    return;
  }

  SetState(LocalAuthenticationWithPinState::kPasswordAuthStarted);
  CHECK(user_context_);
  const auto* password_factor =
      user_context_->GetAuthFactorsData().FindAnyPasswordFactor();
  CHECK(password_factor);

  const cryptohome::KeyLabel key_label = password_factor->ref().label();

  auth_performer_->AuthenticateWithPassword(
      key_label.value(), password_utf8, std::move(user_context_),
      base::BindOnce(&LocalAuthenticationWithPinControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPassword));
}

void LocalAuthenticationWithPinControllerImpl::OnPinSubmit(
    const std::u16string& pin) {
  if (IsSucceedState()) {
    return;
  }

  CHECK_EQ(state_, LocalAuthenticationWithPinState::kInitialized);

  std::string pin_utf8;
  bool conversion_succeeded =
      base::UTF16ToUTF8(pin.data(), pin.length(), &pin_utf8);

  if (!conversion_succeeded) {
    contents_view_->SetErrorTitle(
        l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_INVALID));
    return;
  }

  SetState(LocalAuthenticationWithPinState::kPinAuthStarted);
  CHECK(user_context_);
  const std::string salt = GetUserSalt(account_id_);

  auth_performer_->AuthenticateWithPin(
      pin_utf8, salt, std::move(user_context_),
      base::BindOnce(&LocalAuthenticationWithPinControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPin));
}

void LocalAuthenticationWithPinControllerImpl::OnAuthComplete(
    AuthInputType input_type,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  user_context_ = std::move(user_context);
  CHECK(!IsSucceedState());
  if (state_ == LocalAuthenticationWithPinState::kCloseRequested) {
    StartClose();
    return;
  }
  if (authentication_error.has_value()) {
    contents_view_->SetErrorTitle(l10n_util::GetStringUTF16(
        input_type == AuthInputType::kPassword
            ? IDS_ASH_IN_SESSION_AUTH_PASSWORD_INCORRECT
            : IDS_ASH_IN_SESSION_AUTH_PIN_INCORRECT));
    SetState(LocalAuthenticationWithPinState::kInitialized);
  } else {
    SetState(input_type == AuthInputType::kPassword
                 ? LocalAuthenticationWithPinState::kPasswordAuthSucceeded
                 : LocalAuthenticationWithPinState::kPinAuthSucceeded);
    StartClose();
  }
}

void LocalAuthenticationWithPinControllerImpl::OnClose() {
  switch (state_) {
    case LocalAuthenticationWithPinState::kWaitForInit:
      NOTREACHED();
    case LocalAuthenticationWithPinState::kInitialized:
      StartClose();
      return;
    case LocalAuthenticationWithPinState::kPasswordAuthStarted:
    case LocalAuthenticationWithPinState::kPinAuthStarted:
      SetState(LocalAuthenticationWithPinState::kCloseRequested);
      return;
    case LocalAuthenticationWithPinState::kPasswordAuthSucceeded:
    case LocalAuthenticationWithPinState::kPinAuthSucceeded:
    case LocalAuthenticationWithPinState::kCloseRequested:
      return;
  }
  NOTREACHED();
}

void LocalAuthenticationWithPinControllerImpl::SetState(
    LocalAuthenticationWithPinState state) {
  VLOG(1) << "SetState is requested from: "
          << LocalAuthenticationWithPinStateToString(state_)
          << " state to : " << LocalAuthenticationWithPinStateToString(state)
          << " state.";
  switch (state) {
    case LocalAuthenticationWithPinState::kWaitForInit:
      break;
    case LocalAuthenticationWithPinState::kInitialized:
      CHECK(state_ == LocalAuthenticationWithPinState::kWaitForInit ||
            state_ == LocalAuthenticationWithPinState::kPasswordAuthStarted ||
            state_ == LocalAuthenticationWithPinState::kPinAuthStarted);
      contents_view_->SetInputEnabled(true);
      break;
    case LocalAuthenticationWithPinState::kPasswordAuthStarted:
      // Disable the UI while we are waiting for the response, except the close
      // button.
      CHECK_EQ(state_, LocalAuthenticationWithPinState::kInitialized);
      contents_view_->SetInputEnabled(false);
      break;
    case LocalAuthenticationWithPinState::kPasswordAuthSucceeded:
      CHECK_EQ(state_, LocalAuthenticationWithPinState::kPasswordAuthStarted);
      break;
    case LocalAuthenticationWithPinState::kPinAuthStarted:
      CHECK_EQ(state_, LocalAuthenticationWithPinState::kInitialized);
      contents_view_->SetInputEnabled(false);
      break;
    case LocalAuthenticationWithPinState::kPinAuthSucceeded:
      CHECK_EQ(state_, LocalAuthenticationWithPinState::kPinAuthStarted);
      break;
    case LocalAuthenticationWithPinState::kCloseRequested:
      CHECK(state_ == LocalAuthenticationWithPinState::kPasswordAuthStarted ||
            state_ == LocalAuthenticationWithPinState::kPinAuthStarted);
      contents_view_->SetInputEnabled(false);
      break;
  }
  state_ = state;
}

void LocalAuthenticationWithPinControllerImpl::OnAuthFactorStatusUpdate(
    const user_data_auth::AuthFactorStatusUpdate& update) {
  switch (state_) {
    case LocalAuthenticationWithPinState::kInitialized:
    case LocalAuthenticationWithPinState::kPinAuthStarted:
    case LocalAuthenticationWithPinState::kPasswordAuthStarted:
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

    case LocalAuthenticationWithPinState::kWaitForInit:
      return;

    case LocalAuthenticationWithPinState::kPasswordAuthSucceeded:
    case LocalAuthenticationWithPinState::kPinAuthSucceeded:
    case LocalAuthenticationWithPinState::kCloseRequested:
      // No need to handle PIN updates as dialog closing is in progress.
      return;
  }
  NOTREACHED();
}

}  // namespace ash
