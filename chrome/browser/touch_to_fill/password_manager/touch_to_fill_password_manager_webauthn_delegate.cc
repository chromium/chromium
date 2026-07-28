// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_webauthn_delegate.h"

#include <algorithm>
#include <utility>
#include <variant>

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
#include "url/origin.h"

using Credential = TouchToFillPasswordManagerView::Credential;

TouchToFillPasswordManagerWebAuthnDelegate::
    TouchToFillPasswordManagerWebAuthnDelegate(
        CredentialReceiver* receiver,
        SortingCallback sort_credentials_callback,
        bool should_show_hybrid_option,
        bool is_immediate)
    : credential_receiver_(receiver),
      sort_credentials_callback_(std::move(sort_credentials_callback)),
      should_show_hybrid_option_(should_show_hybrid_option),
      is_immediate_(is_immediate) {}

TouchToFillPasswordManagerWebAuthnDelegate::
    ~TouchToFillPasswordManagerWebAuthnDelegate() = default;

void TouchToFillPasswordManagerWebAuthnDelegate::OnShow(
    base::span<const Credential> credentials) {}

void TouchToFillPasswordManagerWebAuthnDelegate::OnCredentialSelected(
    const password_manager::UiCredential& credential,
    base::OnceClosure action_complete) {
  CHECK(is_immediate_);
  credential_receiver_->OnPasswordCredentialSelected(
      {credential.username(), credential.password()});
  std::move(action_complete).Run();
}

void TouchToFillPasswordManagerWebAuthnDelegate::OnPasskeyCredentialSelected(
    const password_manager::PasskeyCredential& credential,
    base::OnceClosure action_complete) {
  credential_receiver_->OnWebAuthnAccountSelected(credential.credential_id());
  std::move(action_complete).Run();
}

void TouchToFillPasswordManagerWebAuthnDelegate::OnManagePasswordsSelected(
    bool passkeys_shown,
    base::OnceClosure action_complete) {
  password_manager_launcher::ShowPasswordSettings(
      credential_receiver_->web_contents(),
      password_manager::ManagePasswordsReferrer::kTouchToFill,
      /*manage_passkeys=*/true);
  OnDismiss(std::move(action_complete));
}

void TouchToFillPasswordManagerWebAuthnDelegate::OnHybridSignInSelected(
    base::OnceClosure action_complete) {
  credential_receiver_->OnHybridSignInSelected();
  std::move(action_complete).Run();
}

void TouchToFillPasswordManagerWebAuthnDelegate::OnDismiss(
    base::OnceClosure action_complete) {
  credential_receiver_->OnCredentialSelectionDeclined();
  std::move(action_complete).Run();
}

void TouchToFillPasswordManagerWebAuthnDelegate::OnCredManDismissed(
    base::OnceClosure action_completed) {
  std::move(action_completed).Run();
}

GURL TouchToFillPasswordManagerWebAuthnDelegate::GetFrameUrl() const {
  return credential_receiver_ ? credential_receiver_->GetFrameUrl() : GURL();
}

url::Origin TouchToFillPasswordManagerWebAuthnDelegate::GetFrameOrigin() const {
  return credential_receiver_ ? credential_receiver_->GetFrameOrigin()
                              : url::Origin();
}

bool TouchToFillPasswordManagerWebAuthnDelegate::ShouldShowTouchToFill() {
  return true;
}

bool TouchToFillPasswordManagerWebAuthnDelegate::ShouldTriggerSubmission() {
  return false;
}

bool TouchToFillPasswordManagerWebAuthnDelegate::ShouldShowHybridOption() {
  return should_show_hybrid_option_;
}

bool TouchToFillPasswordManagerWebAuthnDelegate::
    ShouldShowNoPasskeysSheetIfRequired() {
  return webauthn::WebAuthnCredManDelegate::CredManMode() ==
         webauthn::WebAuthnCredManDelegate::kNonGpmPasskeys;
}

std::optional<std::vector<Credential>>
TouchToFillPasswordManagerWebAuthnDelegate::SortCredentials(
    base::span<const Credential> credentials) {
  if (sort_credentials_callback_.is_null() || !is_immediate_) {
    return std::nullopt;
  }
  std::vector<Credential> credentials_copy(credentials.begin(),
                                           credentials.end());
  return sort_credentials_callback_.Run(std::move(credentials_copy),
                                        is_immediate_);
}

gfx::NativeView TouchToFillPasswordManagerWebAuthnDelegate::GetNativeView() {
  return credential_receiver_->web_contents()->GetNativeView();
}
