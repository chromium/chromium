// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/android/no_passkeys_bottom_sheet_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view_factory.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
class KeyboardReplacingSurfaceVisibilityController;
class ContentPasswordManagerDriver;
}  // namespace password_manager

namespace webauthn {
class WebAuthnCredManDelegate;
}  // namespace webauthn

class Profile;
class TouchToFillControllerDelegate;

class TouchToFillController final {
 public:
  // Convenience enum for selecting the correct UI that this controller can
  // display.
  enum DisplayTarget {
    kNone,
    kShowTouchToFill,
    kDeferToCredMan,
    kShowNoPasskeysSheet,
  };

  explicit TouchToFillController(
      Profile* profile,
      base::WeakPtr<
          password_manager::KeyboardReplacingSurfaceVisibilityController>
          visibility_controller);
  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController();

  // Instructs the controller to show the provided |credentials| and
  // |passkey_credentials| to the user.
  bool Show(base::span<const password_manager::UiCredential> credentials,
            base::span<password_manager::PasskeyCredential> passkey_credentials,
            std::unique_ptr<TouchToFillControllerDelegate> ttf_delegate,
            webauthn::WebAuthnCredManDelegate* cred_man_delegate,
            base::WeakPtr<password_manager::ContentPasswordManagerDriver>
                frame_driver);

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

  // Informs the controller that the user has tapped the "Use Passkey on a
  // Different Device" option, which initiates hybrid passkey sign-in.
  void OnHybridSignInSelected();

  // Informs the controller that the user selected "More passkeys". This will
  // trigger Android Credential Manager UI. Android U+ only.
  void OnShowCredManSelected();

  // Informs the controller that the Android Credential Manager UI is closed.
  // Android U+ only.
  void OnCredManUiClosed(bool success);

  // Informs the controller that the user has dismissed the sheet. No-op if
  // invoked repeatedly.
  void OnDismiss();

  // The Profile associated with the credentials.
  Profile* GetProfile();

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Called by the owner to dismiss the sheet without waiting for user
  // interaction.
  void Close();

  // Resets TTF to the state as if it was never shown.
  void Reset();

#if defined(UNIT_TEST)
  void set_view(std::unique_ptr<TouchToFillView> view) {
    view_ = std::move(view);
  }

  void set_no_passkeys_bridge(
      std::unique_ptr<NoPasskeysBottomSheetBridge> bridge) {
    no_passkeys_bridge_ = std::move(bridge);
  }
#endif

 private:
  // Callback method for the delegate to signal that it has completed its
  // action and is no longer needed. This destroys the delegate.
  void ActionCompleted();

  // Helper method to select the display target.
  DisplayTarget GetResponsibleDisplayTarget(
      base::span<const password_manager::UiCredential> credentials,
      base::span<password_manager::PasskeyCredential> passkey_credentials)
      const;

  // Delegate for interacting with the client that owns this controller.
  // It is provided when Show() is called, and reset when the view is
  // destroyed.
  std::unique_ptr<TouchToFillControllerDelegate> ttf_delegate_;

  // Delegate for interacting with the Android Credential Manager. Lifecycle is
  // not bound to this controller.
  raw_ptr<webauthn::WebAuthnCredManDelegate> cred_man_delegate_;
  // View used to communicate with the Android frontend. Lazily instantiated so
  // that it can be injected by tests.
  std::unique_ptr<TouchToFillView> view_;

  std::unique_ptr<NoPasskeysBottomSheetBridge> no_passkeys_bridge_;

  raw_ptr<Profile> profile_;

  base::WeakPtr<password_manager::KeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;

  base::WeakPtrFactory<TouchToFillController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
