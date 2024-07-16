// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"

namespace password_manager {

using ToShowVirtualKeyboard = PasswordManagerDriver::ToShowVirtualKeyboard;
using webauthn::WebAuthnCredManDelegate;

CredManController::CredManController(
    base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
        visibility_controller,
    password_manager::PasswordManagerClient* password_client)
    : visibility_controller_(visibility_controller),
      password_client_(password_client),
      authenticator_(password_client->GetDeviceAuthenticator()) {}

CredManController::~CredManController() {
  if (visibility_controller_) {
    visibility_controller_->Reset();
  }
  if (authenticator_) {
    authenticator_->Cancel();
  }
}

bool CredManController::Show(
    raw_ptr<WebAuthnCredManDelegate> cred_man_delegate,
    std::unique_ptr<PasswordCredentialFiller> filler,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> frame_driver,
    bool is_webauthn_form) {
  // webauthn forms without passkeys should show TouchToFill bottom sheet.
  if (!cred_man_delegate || !is_webauthn_form ||
      WebAuthnCredManDelegate::CredManMode() !=
          WebAuthnCredManDelegate::CredManEnabledMode::kAllCredMan ||
      cred_man_delegate->HasPasskeys() !=
          WebAuthnCredManDelegate::State::kHasPasskeys) {
    filler->Dismiss(ToShowVirtualKeyboard(false));
    return false;
  }
  visibility_controller_->SetVisible(std::move(frame_driver));
  filler_ = std::move(filler);
  cred_man_delegate->SetRequestCompletionCallback(base::BindRepeating(
      &CredManController::Dismiss, weak_ptr_factory_.GetWeakPtr()));
  cred_man_delegate->SetFillingCallback(base::BindOnce(
      &CredManController::TriggerFilling, weak_ptr_factory_.GetWeakPtr()));
  cred_man_delegate->TriggerCredManUi(
      WebAuthnCredManDelegate::RequestPasswords(true));
  return true;
}

void CredManController::Dismiss(bool success) {
  if (visibility_controller_) {
    visibility_controller_->SetShown();
  }
  if (filler_) {
    // If |success|, we do not need to show the keyboard. Request to show the
    // keyboard for user convenience.
    filler_->Dismiss(ToShowVirtualKeyboard(!success));
  }
}

void CredManController::TriggerFilling(const std::u16string& username,
                                       const std::u16string& password) {
  if (!filler_ || !visibility_controller_) {
    return;
  }
  visibility_controller_->SetShown();
  if (!password_client_->IsReauthBeforeFillingRequired(authenticator_.get())) {
    FillUsernameAndPassword(username, password);
    return;
  }
  authenticator_->AuthenticateWithMessage(
      u"", base::BindOnce(&CredManController::OnReauthCompleted,
                          base::Unretained(this), username, password));
}

void CredManController::FillUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password) {
  filler_->FillUsernameAndPassword(username, password);
  base::UmaHistogramBoolean(
      "PasswordManager.CredMan.PasswordFormSubmissionTriggered",
      filler_->ShouldTriggerSubmission());
}

void CredManController::OnReauthCompleted(const std::u16string& username,
                                          const std::u16string& password,
                                          bool auth_successful) {
  if (!auth_successful) {
    return;
  }
  FillUsernameAndPassword(username, password);
}

}  // namespace password_manager
