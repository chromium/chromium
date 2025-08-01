// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

TouchToFillControllerWebAuthnDelegate::TouchToFillControllerWebAuthnDelegate(
    CredentialReceiver* receiver,
    bool should_show_hybrid_option,
    bool is_immediate)
    : credential_receiver_(receiver),
      should_show_hybrid_option_(should_show_hybrid_option),
      is_immediate_(is_immediate) {}

TouchToFillControllerWebAuthnDelegate::
    ~TouchToFillControllerWebAuthnDelegate() = default;

void TouchToFillControllerWebAuthnDelegate::OnShow(
    base::span<const password_manager::UiCredential> credentials,
    base::span<password_manager::PasskeyCredential> webauthn_credentials) {}

void TouchToFillControllerWebAuthnDelegate::OnCredentialSelected(
    const password_manager::UiCredential& credential,
    base::OnceClosure action_complete) {
  CHECK(is_immediate_);
  credential_receiver_->OnPasswordCredentialSelected(
      {credential.username(), credential.password()});
  std::move(action_complete).Run();
}

void TouchToFillControllerWebAuthnDelegate::OnPasskeyCredentialSelected(
    const password_manager::PasskeyCredential& credential,
    base::OnceClosure action_complete) {
  credential_receiver_->OnWebAuthnAccountSelected(credential.credential_id());
  std::move(action_complete).Run();
}

void TouchToFillControllerWebAuthnDelegate::OnManagePasswordsSelected(
    bool passkeys_shown,
    base::OnceClosure action_complete) {
  password_manager_launcher::ShowPasswordSettings(
      credential_receiver_->web_contents(),
      password_manager::ManagePasswordsReferrer::kTouchToFill,
      /*manage_passkeys=*/true);
  OnDismiss(std::move(action_complete));
}

void TouchToFillControllerWebAuthnDelegate::OnHybridSignInSelected(
    base::OnceClosure action_complete) {
  credential_receiver_->OnHybridSignInSelected();
  std::move(action_complete).Run();
}

void TouchToFillControllerWebAuthnDelegate::OnDismiss(
    base::OnceClosure action_complete) {
  if (is_immediate_) {
    credential_receiver_->OnCredentialSelectionDeclined();
  } else {
    credential_receiver_->OnWebAuthnAccountSelected(std::vector<uint8_t>());
  }
  std::move(action_complete).Run();
}

void TouchToFillControllerWebAuthnDelegate::OnCredManDismissed(
    base::OnceClosure action_completed) {
  std::move(action_completed).Run();
}

GURL TouchToFillControllerWebAuthnDelegate::GetFrameUrl() {
  return credential_receiver_->web_contents()->GetLastCommittedURL();
}

bool TouchToFillControllerWebAuthnDelegate::ShouldShowTouchToFill() {
  return true;
}

bool TouchToFillControllerWebAuthnDelegate::ShouldTriggerSubmission() {
  return false;
}

bool TouchToFillControllerWebAuthnDelegate::ShouldShowHybridOption() {
  return should_show_hybrid_option_;
}

bool TouchToFillControllerWebAuthnDelegate::
    ShouldShowNoPasskeysSheetIfRequired() {
  return webauthn::WebAuthnCredManDelegate::CredManMode() ==
         webauthn::WebAuthnCredManDelegate::kNonGpmPasskeys;
}

gfx::NativeView TouchToFillControllerWebAuthnDelegate::GetNativeView() {
  return credential_receiver_->web_contents()->GetNativeView();
}
