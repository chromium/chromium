// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_webauthn_delegate.h"

#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

TouchToFillControllerWebAuthnDelegate::TouchToFillControllerWebAuthnDelegate(
    WebAuthnRequestDelegateAndroid* request_delegate)
    : request_delegate_(request_delegate) {}

TouchToFillControllerWebAuthnDelegate::
    ~TouchToFillControllerWebAuthnDelegate() = default;

void TouchToFillControllerWebAuthnDelegate::OnShow(
    base::span<const password_manager::UiCredential> credentials,
    base::span<password_manager::PasskeyCredential> webauthn_credentials) {}

void TouchToFillControllerWebAuthnDelegate::OnCredentialSelected(
    const password_manager::UiCredential& credential,
    base::OnceClosure action_complete) {
  NOTIMPLEMENTED();
}

void TouchToFillControllerWebAuthnDelegate::OnPasskeyCredentialSelected(
    const password_manager::PasskeyCredential& credential,
    base::OnceClosure action_complete) {
  request_delegate_->OnWebAuthnAccountSelected(
      *base::Base64Decode(credential.id().value()));
  std::move(action_complete).Run();
}

void TouchToFillControllerWebAuthnDelegate::OnManagePasswordsSelected(
    base::OnceClosure action_complete) {
  password_manager_launcher::ShowPasswordSettings(
      request_delegate_->web_contents(),
      password_manager::ManagePasswordsReferrer::kTouchToFill);
  OnDismiss(std::move(action_complete));
}

void TouchToFillControllerWebAuthnDelegate::OnDismiss(
    base::OnceClosure action_complete) {
  request_delegate_->OnWebAuthnAccountSelected(std::vector<uint8_t>());
  std::move(action_complete).Run();
}

const GURL& TouchToFillControllerWebAuthnDelegate::GetFrameUrl() {
  return request_delegate_->web_contents()->GetLastCommittedURL();
}

bool TouchToFillControllerWebAuthnDelegate::ShouldTriggerSubmission() {
  return false;
}

gfx::NativeView TouchToFillControllerWebAuthnDelegate::GetNativeView() {
  return request_delegate_->web_contents()->GetNativeView();
}
