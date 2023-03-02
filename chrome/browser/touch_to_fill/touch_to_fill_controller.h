// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}  // namespace password_manager

class TouchToFillControllerDelegate;

class TouchToFillController {
 public:
  TouchToFillController();
  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController();

  // Instructs the controller to show the provided |credentials| and
  // |passkey_credentials| to the user.
  void Show(base::span<const password_manager::UiCredential> credentials,
            base::span<password_manager::PasskeyCredential> passkey_credentials,
            std::unique_ptr<TouchToFillControllerDelegate> delegate);

  // Informs the controller that the user has made a selection. Invokes both
  // FillSuggestion() and TouchToFillDismissed() on |driver_|. No-op if invoked
  // repeatedly.
  void OnCredentialSelected(const password_manager::UiCredential& credential);

  // Informs the controller that the user has selected a passkey. Invokes
  // TouchToFillDismissed() and initiates a WebAuthn sign-in.
  void OnPasskeyCredentialSelected(
      const password_manager::PasskeyCredential& credential);

  // Informs the controller that the user has tapped the "Manage Passwords"
  // button. This will open the password preferences. |passkeys_shown|
  // indicates passkeys were displayed to the user, which can affect which
  // password management screen is displayed.
  void OnManagePasswordsSelected(bool passkeys_shown);

  // Informs the controller that the user has dismissed the sheet. No-op if
  // invoked repeatedly.
  void OnDismiss();

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Called by the owner to dismiss the sheet without waiting for user
  // interaction.
  void Close();

#if defined(UNIT_TEST)
  void set_view(std::unique_ptr<TouchToFillView> view) {
    view_ = std::move(view);
  }
#endif

 private:
  // Callback method for the delegate to signal that it has completed its
  // action and is no longer needed. This destroys the delegate.
  void ActionCompleted();

  // Delegate for interacting with the client that owns this controller.
  // It is provided when Show() is called, and reset when the view is
  // destroyed.
  std::unique_ptr<TouchToFillControllerDelegate> delegate_;

  // View used to communicate with the Android frontend. Lazily instantiated so
  // that it can be injected by tests.
  std::unique_ptr<TouchToFillView> view_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_
