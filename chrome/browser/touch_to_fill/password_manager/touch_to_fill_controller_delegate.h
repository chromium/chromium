// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_DELEGATE_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}

class GURL;

// Delegate interface for TouchToFillController, to be implemented by
// owner-specified classes.
class TouchToFillControllerDelegate {
 public:
  virtual ~TouchToFillControllerDelegate();

  // Called by the controller before the view is shown.
  virtual void OnShow(
      base::span<const password_manager::UiCredential> credentials,
      base::span<password_manager::PasskeyCredential> passkey_credentials) = 0;

  // Informs the controller that the user has made a selection. Invokes both
  // FillSuggestion() and TouchToFillDismissed() on |driver_|. No-op if
  // invoked repeatedly.
  virtual void OnCredentialSelected(
      const password_manager::UiCredential& credential,
      base::OnceClosure action_completed) = 0;

  // Informs the controller that the user has selected a passkey. Invokes
  // TouchToFillDismissed() and initiates a WebAuthn sign-in.
  virtual void OnPasskeyCredentialSelected(
      const password_manager::PasskeyCredential& credential,
      base::OnceClosure action_completed) = 0;

  // Informs the controller that the user has tapped the "Manage Passwords"
  // button. This will open the password preferences of universal password
  // manager.
  virtual void OnManagePasswordsSelected(
      bool passkeys_shown,
      base::OnceClosure action_completed) = 0;

  virtual void OnHybridSignInSelected(base::OnceClosure action_completed) = 0;

  // Informs the controller that the user has dismissed the sheet. No-op if
  // invoked repeatedly.
  virtual void OnDismiss(base::OnceClosure action_completed) = 0;

  // Informs the controller that the user has dismissed the Android Credential
  // Manager UI.
  virtual void OnCredManDismissed(base::OnceClosure action_completed) = 0;

  // Gets the last committed URL for the frame that triggered this sheet to be
  // created.
  virtual GURL GetFrameUrl() = 0;

  // Returns whether TTF is eligible for showing for the currently focused field
  // (e. g. it should not be triggered for the new password field).
  virtual bool ShouldShowTouchToFill() = 0;

  // Indicates whether the controller should trigger submission on selection of
  // a password credential.
  virtual bool ShouldTriggerSubmission() = 0;

  // Indicates whether the view should display an option to activate hybrid
  // sign-in for passkeys.
  virtual bool ShouldShowHybridOption() = 0;

  // Indicates if the delegate requires displaying a no passkeys bottom sheet
  // when the touch to fill component does not have any credentials to display.
  virtual bool ShouldShowNoPasskeysSheetIfRequired() = 0;

  // The web page view containing the focused field.
  virtual gfx::NativeView GetNativeView() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_DELEGATE_H_
