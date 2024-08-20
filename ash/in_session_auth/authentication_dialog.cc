// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/authentication_dialog.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

void AddMargins(views::View* view) {
  const auto* layout_provider = views::LayoutProvider::Get();
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  view->SetProperty(views::kMarginsKey,
                    gfx::Insets::VH(vertical_spacing, horizontal_spacing));
}

void ConfigurePasswordField(views::Textfield* password_field) {
  const auto password_field_name =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER);
  password_field->GetViewAccessibility().SetName(password_field_name);
  password_field->SetReadOnly(false);
  password_field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD);
  password_field->SetPlaceholderText(password_field_name);
  AddMargins(password_field);
}

void ConfigureInvalidPasswordLabel(views::Label* invalid_password_label) {
  invalid_password_label->SetProperty(views::kCrossAxisAlignmentKey,
                                      views::LayoutAlignment::kStart);
  invalid_password_label->SetEnabledColor(SK_ColorRED);
  AddMargins(invalid_password_label);
}

void CenterWidgetOnPrimaryDisplay(views::Widget* widget) {
  auto bounds = display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  bounds.ClampToCenteredSize(widget->GetContentsView()->GetPreferredSize());
  widget->SetBounds(bounds);
}

}  // namespace

AuthenticationDialog::AuthenticationDialog(
    auth_panel::AuthCompletionCallback on_auth_complete,
    InSessionAuthTokenProvider* auth_token_provider,
    std::unique_ptr<AuthPerformer> auth_performer,
    const AccountId& account_id)
    : password_field_(AddChildView(std::make_unique<views::Textfield>())),
      invalid_password_label_(AddChildView(std::make_unique<views::Label>())),
      on_auth_complete_(std::move(on_auth_complete)),
      auth_performer_(std::move(auth_performer)),
      auth_token_provider_(auth_token_provider) {
  // Dialog setup
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetTitle(l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE));
  SetModalType(ui::mojom::ModalType::kSystem);

  // Callback setup
  SetCancelCallback(base::BindOnce(&AuthenticationDialog::CancelAuthAttempt,
                                   base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&AuthenticationDialog::CancelAuthAttempt,
                                  base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);

  ConfigureChildViews();

  // We don't want the user to submit an auth factor to cryptohome before the
  // auth session has started. We re-enable the UI in `OnAuthSessionStarted`
  SetUIDisabled(true);

  auto user_context = std::make_unique<UserContext>();
  user_context->SetAccountId(account_id);

  // TODO(b/240147756): Choose the intent based on
  // `InSessionAuthDialogController::Reason`.
  auth_performer_->StartAuthSession(
      std::move(user_context), /*ephemeral=*/false, AuthSessionIntent::kDecrypt,
      base::BindOnce(&AuthenticationDialog::OnAuthSessionStarted,
                     weak_factory_.GetWeakPtr()));
}

AuthenticationDialog::~AuthenticationDialog() = default;

void AuthenticationDialog::Show() {
  auto* widget = DialogDelegateView::CreateDialogWidget(this,
                                                        /*context=*/nullptr,
                                                        /*parent=*/nullptr);
  CenterWidgetOnPrimaryDisplay(widget);
  Init();
  widget->Show();
}

void AuthenticationDialog::Init() {
  ConfigureOkButton();
  password_field_->RequestFocus();
}

void AuthenticationDialog::NotifyResult(bool success,
                                        const AuthProofToken& token,
                                        base::TimeDelta timeout) {
  if (on_auth_complete_) {
    std::move(on_auth_complete_).Run(success, token, timeout);
  }
}

void AuthenticationDialog::ConfigureOkButton() {
  views::LabelButton* ok_button = GetOkButton();
  ok_button->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  ok_button->SetCallback(base::BindRepeating(
      &AuthenticationDialog::ValidateAuthFactor, weak_factory_.GetWeakPtr()));
}

void AuthenticationDialog::SetUIDisabled(bool is_disabled) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, !is_disabled);
  SetButtonEnabled(ui::mojom::DialogButton::kCancel, !is_disabled);
  password_field_->SetReadOnly(is_disabled);
}

void AuthenticationDialog::ValidateAuthFactor() {
  // Clear warning message.
  invalid_password_label_->SetText({});

  SetUIDisabled(true);

  const auto* password_factor =
      user_context_->GetAuthFactorsData().FindAnyPasswordFactor();
  if (!password_factor) {
    LOG(ERROR) << "Could not find password key";
    ShowAuthError();
    return;
  }

  cryptohome::KeyLabel key_label = password_factor->ref().label();

  // Create a copy of `user_context_` so that we don't lose it to std::move
  // for future auth attempts
  auth_performer_->AuthenticateWithPassword(
      key_label.value(), base::UTF16ToUTF8(password_field_->GetText()),
      std::make_unique<UserContext>(*user_context_),
      base::BindOnce(&AuthenticationDialog::OnAuthFactorValidityChecked,
                     weak_factory_.GetWeakPtr()));
}

void AuthenticationDialog::OnAuthFactorValidityChecked(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    if (cryptohome::ErrorMatches(
            authentication_error.value().get_cryptohome_error(),
            user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN)) {
      // Auth session expired for some reason, start it again and reattempt
      // authentication.
      // TODO(b/240147756): Choose the intent based on
      // `InSessionAuthDialogController::Reason`.
      auth_performer_->StartAuthSession(
          std::move(user_context), /*ephemeral=*/false,
          AuthSessionIntent::kDecrypt,
          base::BindOnce(&AuthenticationDialog::OnAuthSessionInvalid,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    LOG(ERROR) << "An error happened during the attempt to validate"
                  "the password: "
               << authentication_error.value().get_cryptohome_error();
    ShowAuthError();
    return;
  }

  is_closing_ = true;

  auth_token_provider_->ExchangeForToken(
      std::move(user_context),
      base::BindOnce(&AuthenticationDialog::NotifyResult,
                     weak_factory_.GetWeakPtr(), /*success=*/true));

  SetUIDisabled(false);
  CancelDialog();
  return;
}

void AuthenticationDialog::ShowAuthError() {
  password_field_->SetInvalid(true);
  password_field_->SelectAll(false);
  invalid_password_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_AUTHENTICATING));
  SetUIDisabled(false);
}

void AuthenticationDialog::CancelAuthAttempt() {
  // If dialog is closing after the submission of a valid auth factor,
  // we should not notify any parties, as they would have already been
  // notified after `AuthenticationDialog::OnAuthFactorValidityChecked`
  if (!is_closing_) {
    NotifyResult(/*success=*/false, /*token=*/{}, /*timeout=*/{});
  }
}

void AuthenticationDialog::ConfigureChildViews() {
  ConfigurePasswordField(password_field_);
  ConfigureInvalidPasswordLabel(invalid_password_label_);
}

void AuthenticationDialog::OnAuthSessionInvalid(
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  OnAuthSessionStarted(user_exists, std::move(user_context),
                       authentication_error);
  ValidateAuthFactor();
}

void AuthenticationDialog::OnAuthSessionStarted(
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "Error starting authsession for in session authentication: "
               << authentication_error.value().get_cryptohome_error();
    CancelAuthAttempt();
  } else if (!user_exists) {
    LOG(ERROR) << "Attempting to authenticate a user which does not exist. "
                  "Aborting authentication attempt";
    CancelAuthAttempt();
  } else {
    user_context_ = std::move(user_context);
    SetUIDisabled(false);
  }
}

}  // namespace ash
