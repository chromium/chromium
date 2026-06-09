// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_delegate.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "chrome/browser/webauthn/shared_types.h"
#include "chrome/browser/webauthn/touch_to_fill_credential_receiver.h"
#include "ui/gfx/native_ui_types.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}  // namespace password_manager

// Derived TouchToFillController class specific for use with non-conditional
// WebAuthn sign-in. It does not integrate with the password manager because it
// is in response to an immediate WebAuthn invocation, without necessarily any
// forms or input fields being present on the page.
// This is owned by WebAuthnRequestDelegateAndroid.
class TouchToFillControllerWebAuthnDelegate
    : public TouchToFillControllerDelegate {
 public:
  using SortingCallback =
      base::RepeatingCallback<std::vector<TouchToFillView::Credential>(
          std::vector<TouchToFillView::Credential>,
          bool)>;

  using CredentialReceiver = TouchToFillCredentialReceiver;

  TouchToFillControllerWebAuthnDelegate(
      CredentialReceiver* receiver,
      SortingCallback sort_credentials_callback,
      bool should_show_hybrid_option,
      bool is_immediate);

  TouchToFillControllerWebAuthnDelegate(
      const TouchToFillControllerWebAuthnDelegate&) = delete;
  TouchToFillControllerWebAuthnDelegate& operator=(
      const TouchToFillControllerWebAuthnDelegate&) = delete;

  ~TouchToFillControllerWebAuthnDelegate() override;

  // TouchToFillControllerDelegate:
  void OnShow(
      base::span<const TouchToFillView::Credential> credentials) override;
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
  url::Origin GetFrameOrigin() override;
  bool ShouldShowTouchToFill() override;
  bool ShouldTriggerSubmission() override;
  bool ShouldShowHybridOption() override;
  bool ShouldShowNoPasskeysSheetIfRequired() override;
  std::optional<std::vector<TouchToFillView::Credential>> SortCredentials(
      base::span<const TouchToFillView::Credential> credentials) override;
  gfx::NativeView GetNativeView() override;

 private:
  // Raw pointer to the owning class that will receive the selected credential,
  // or other outcomes of the request.
  raw_ptr<CredentialReceiver> credential_receiver_ = nullptr;

  SortingCallback sort_credentials_callback_;

  bool should_show_hybrid_option_;
  bool is_immediate_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_WEBAUTHN_DELEGATE_H_
