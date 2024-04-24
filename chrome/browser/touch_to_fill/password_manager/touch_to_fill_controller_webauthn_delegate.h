// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}  // namespace password_manager

class WebAuthnRequestDelegateAndroid;

// Derived TouchToFillController class specific for use with non-conditional
// WebAuthn sign-in. It does not integrate with the password manager because it
// is in response to an immediate WebAuthn invocation, without necessarily any
// forms or input fields being present on the page.
// This is owned by WebAuthnRequestDelegateAndroid.
class TouchToFillControllerWebAuthnDelegate
    : public TouchToFillControllerDelegate {
 public:
  explicit TouchToFillControllerWebAuthnDelegate(
      WebAuthnRequestDelegateAndroid* delegate,
      bool should_show_hybrid_option);

  TouchToFillControllerWebAuthnDelegate(
      const TouchToFillControllerWebAuthnDelegate&) = delete;
  TouchToFillControllerWebAuthnDelegate& operator=(
      const TouchToFillControllerWebAuthnDelegate&) = delete;

  ~TouchToFillControllerWebAuthnDelegate() override;

  // TouchToFillControllerDelegate:
  void OnShow(base::span<const password_manager::UiCredential> credentials,
              base::span<password_manager::PasskeyCredential>
                  passkey_credentials) override;
  void OnCredentialSelected(const password_manager::UiCredential& credential,
                            base::OnceClosure action_completed) override;
  void OnPasskeyCredentialSelected(
      const password_manager::PasskeyCredential& credential,
      base::OnceClosure action_completed) override;
  void OnManagePasswordsSelected(bool passkeys_shown,
                                 base::OnceClosure action_completed) override;
  void OnHybridSignInSelected(base::OnceClosure action_completed) override;
  void OnDismiss(base::OnceClosure action_completed) override;
  void OnCredManDismissed(base::OnceClosure action_completed) override;
  GURL GetFrameUrl() override;
  bool ShouldShowTouchToFill() override;
  bool ShouldTriggerSubmission() override;
  bool ShouldShowHybridOption() override;
  bool ShouldShowNoPasskeysSheetIfRequired() override;
  gfx::NativeView GetNativeView() override;

 private:
  // Raw pointer to the request delegate that owns this.
  raw_ptr<WebAuthnRequestDelegateAndroid> request_delegate_ = nullptr;

  bool should_show_hybrid_option_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_
