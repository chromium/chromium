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
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/account_id/account_id.h"
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

namespace ash {

namespace {

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
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
  params.delegate->SetModalType(ui::MODAL_TYPE_NONE);
  params.delegate->SetOwnedByWidget(true);

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

}  // namespace

ActiveSessionAuthControllerImpl::ActiveSessionAuthControllerImpl() = default;
ActiveSessionAuthControllerImpl::~ActiveSessionAuthControllerImpl() = default;

bool ActiveSessionAuthControllerImpl::ShowAuthDialog(
    Reason reason,
    const AccountId& account_id,
    AuthCompletionCallback on_auth_complete) {
  if (IsShown()) {
    LOG(ERROR) << "ActiveSessionAuthController widget is already exists.";
    return false;
  }

  title_ = l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE);
  description_ = l10n_util::GetStringUTF16(
      reason == Reason::kSettings
          ? IDS_ASH_IN_SESSION_AUTH_SETTINGS_PROMPT
          : IDS_ASH_IN_SESSION_AUTH_PASSWORD_MANAGER_PROMPT);
  on_auth_complete_ = std::move(on_auth_complete);
  auth_factor_editor_ =
      std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get());
  auth_performer_ = std::make_unique<AuthPerformer>(UserDataAuthClient::Get());
  account_id_ = account_id;

  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  auto user_context = std::make_unique<UserContext>(*active_user);

  auth_factor_editor_->GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthFactorsListed,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

bool ActiveSessionAuthControllerImpl::IsShown() const {
  return widget_ != nullptr;
}

void ActiveSessionAuthControllerImpl::OnAuthFactorsListed(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << authentication_error->get_cryptohome_code();
    Close();
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  AuthFactorSet available_factors;
  if (config.FindFactorByType(cryptohome::AuthFactorType::kPassword)) {
    available_factors.Put(AuthInputType::kPassword);
  }

  if (config.FindFactorByType(cryptohome::AuthFactorType::kPin)) {
    available_factors.Put(AuthInputType::kPin);
  }

  auto contents_view = std::make_unique<ActiveSessionAuthView>(
      account_id_, title_, description_, available_factors);
  contents_view_ = contents_view.get();
  contents_view_->AddObserver(this);

  widget_ = CreateAuthDialogWidget(std::move(contents_view));
  contents_view_observer_.Observe(contents_view_);

  MoveToTheCenter();
  widget_->Show();
}

void ActiveSessionAuthControllerImpl::Close() {
  contents_view_observer_.Reset();
  CHECK(contents_view_);
  contents_view_->RemoveObserver(this);
  contents_view_ = nullptr;

  if (auth_performer_) {
    auth_performer_->InvalidateCurrentAttempts();
    auth_performer_.reset();
  }
  auth_factor_editor_.reset();

  title_.clear();
  description_.clear();

  widget_.reset();

  if (on_auth_complete_) {
    std::move(on_auth_complete_)
        .Run(false, ash::AuthProofToken{}, base::TimeDelta{});
  }
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
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  auto user_context = std::make_unique<UserContext>(*active_user);

  CHECK_EQ(active_user->GetAccountId(), account_id_);

  const bool ephemeral =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          account_id_);

  auth_performer_->StartAuthSession(
      std::move(user_context), ephemeral, AuthSessionIntent::kVerifyOnly,
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnPasswordSessionStarted,
                     weak_ptr_factory_.GetWeakPtr(), password));
}

void ActiveSessionAuthControllerImpl::OnPasswordSessionStarted(
    const std::u16string& password,
    bool exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << authentication_error->get_cryptohome_code();
    // TODO: Is close is the right thing to do?
    Close();
    return;
  }
  const auto* password_factor =
      user_context->GetAuthFactorsData().FindAnyPasswordFactor();
  CHECK(password_factor);

  const cryptohome::KeyLabel key_label = password_factor->ref().label();

  auth_performer_->AuthenticateWithPassword(
      key_label.value(), base::UTF16ToUTF8(password), std::move(user_context),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPassword));
}

void ActiveSessionAuthControllerImpl::OnPinSubmit(const std::u16string& pin) {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  auto user_context = std::make_unique<UserContext>(*active_user);

  CHECK_EQ(active_user->GetAccountId(), account_id_);

  const bool ephemeral =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          account_id_);

  auth_performer_->StartAuthSession(
      std::move(user_context), ephemeral, AuthSessionIntent::kVerifyOnly,
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnPinSessionStarted,
                     weak_ptr_factory_.GetWeakPtr(), pin));
}

void ActiveSessionAuthControllerImpl::OnPinSessionStarted(
    const std::u16string& pin,
    bool exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << authentication_error->get_cryptohome_code();
    // TODO: Is close is the right thing to do?
    Close();
    return;
  }

  // TODO: Is this correct?!
  user_manager::KnownUser known_user(Shell::Get()->local_state());
  const std::string salt =
      *known_user.FindStringPath(account_id_, prefs::kQuickUnlockPinSalt);

  auth_performer_->AuthenticateWithPin(
      base::UTF16ToUTF8(pin), salt, std::move(user_context),
      base::BindOnce(&ActiveSessionAuthControllerImpl::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr(), AuthInputType::kPin));
}

void ActiveSessionAuthControllerImpl::OnAuthComplete(
    AuthInputType input_type,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    contents_view_->SetErrorTitle(l10n_util::GetStringUTF16(
        input_type == AuthInputType::kPassword
            ? IDS_ASH_IN_SESSION_AUTH_PASSWORD_INCORRECT
            : IDS_ASH_IN_SESSION_AUTH_PIN_INCORRECT));
  } else {
    ExchangeForToken(
        std::move(user_context),
        base::BindOnce(&ActiveSessionAuthControllerImpl::NotifySuccess,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ActiveSessionAuthControllerImpl::ExchangeForToken(
    std::unique_ptr<UserContext> user_context,
    InSessionAuthTokenProvider::OnAuthTokenGenerated callback) {
  AuthProofToken token =
      AuthSessionStorage::Get()->Store(std::move(user_context));
  std::move(callback).Run(token, cryptohome::kAuthsessionInitialLifetime);
}

void ActiveSessionAuthControllerImpl::NotifySuccess(const AuthProofToken& token,
                                                    base::TimeDelta timeout) {
  CHECK(on_auth_complete_);
  std::move(on_auth_complete_).Run(true, token, timeout);
  Close();
}

void ActiveSessionAuthControllerImpl::OnClose() {
  Close();
}

}  // namespace ash
