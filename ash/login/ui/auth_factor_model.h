// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_

#include "ash/ash_export.h"
#include "base/callback.h"

namespace ash {

class AuthIconView;

enum class AuthFactorType {
  kFingerprint = 1 << 0,
  kSmartLock = 1 << 1,
};

using AuthFactorTypeBits = int;

// Allow combining AuthFactorTypes with bitwise OR. Used to look up the
// appropriate label to show when several auth factors are visible.
constexpr AuthFactorTypeBits operator|(AuthFactorTypeBits types,
                                       AuthFactorType type) {
  return types | static_cast<AuthFactorTypeBits>(type);
}
constexpr AuthFactorTypeBits operator|(AuthFactorType type1,
                                       AuthFactorType type2) {
  return static_cast<AuthFactorTypeBits>(type1) |
         static_cast<AuthFactorTypeBits>(type2);
}

// Base class representing an auth factor. Used by LoginAuthFactorsView to
// display a list of auth factors.
class ASH_EXPORT AuthFactorModel {
 public:
  enum class AuthFactorState {
    // The feature is disabled, disallowed by policy, or requires
    // hardware that isn’t present.
    kUnavailable,
    // The auth factor cannot be used because of an unrecoverable
    // error, e.g. Fingerprint’s “Too many attempts”. GetLabel()
    // and UpdateIcon() show the relevant messages.
    kErrorPermanent,
    // The auth factor can be used but requires additional steps
    // before use, e.g. turn on Bluetooth.
    kAvailable,
    // The auth factor is ready to authenticate. This state should
    // only be returned if authentication can be completed in one
    // step (two if a click is required).
    kReady,
    // The auth factor has a non-blocking error to show the
    // user, e.g. Fingerprint’s “Not recognized”, which clears
    // after a few seconds. GetLabel() and UpdateIcon() show the
    // relevant messages.
    kErrorTemporary,
    // The auth factor requires the user to tap/click to enter.
    kClickRequired,
    // Authentication is complete.
    kAuthenticated,
  };

  // Should be called when the visibility of PIN authentication changes.
  static void set_can_use_pin(bool can_use_pin);

  static bool can_use_pin();

  AuthFactorModel();
  AuthFactorModel(AuthFactorModel&) = delete;
  AuthFactorModel& operator=(AuthFactorModel&) = delete;
  virtual ~AuthFactorModel();

  // Initializes |icon_| and |update_state_callback_|. Should be called
  // exactly once before any other methods. The |update_state_callback| is
  // used by LoginAuthFactorsView to determine when it is necessary to update
  // the displayed icons and label.
  // TODO(crbug.com/1233614): Refactor to disallow the creation of partially
  // initialized objects.
  void Init(AuthIconView* icon, base::RepeatingClosure update_state_callback);

  // Set the visibility of the associated icon.
  void SetVisible(bool visible);

  // Should be called when the parent View's theme changes.
  void OnThemeChanged();

  // Should be called when the auth factor's icon is tapped or clicked.
  void HandleTapOrClick();

  // Should be called after an error is shown to the user.
  void HandleErrorTimeout();

  // Return the current state of this auth factor.
  virtual AuthFactorState GetAuthFactorState() const = 0;

  // Returns the type of the auth factor. Each implementation of AuthFactorModel
  // should add a new type to the AuthFactorType enum.
  virtual AuthFactorType GetType() const = 0;

  // The ID of the label that should be shown in the current state.
  virtual int GetLabelId() const = 0;

  // Controls whether the label is announced by Chromevox.
  virtual bool ShouldAnnounceLabel() const = 0;

  // Alternative text to be provided to screen readers.
  virtual int GetAccessibleNameId() const = 0;

  // This will be called when the arrow button owned by `LoginAuthFactorsView`
  // is tapped or clicked.
  virtual void OnArrowButtonTapOrClickEvent();

  // If the auth factor state is kErrorPermanent, this indicates whether the
  // error has been shown to the user and timed out.
  bool has_permanent_error_display_timed_out() const {
    return has_permanent_error_display_timed_out_;
  }

 protected:
  // Should be called whenever the internal state of the auth model changes to
  // invoke the |update_state_callback_| if set. Calls `UpdateIcon`.
  void RefreshUI();

  // Indicates whether PIN is visible on the lock/login screen. May be used to
  // show different labels depending on whether PIN is available.
  static bool can_use_pin_;

  // If the auth factor state is kErrorPermanent, this indicates whether the
  // error has been shown to the user and timed out, which determines whether
  // the error icon or the disabled icon should be shown.
  bool has_permanent_error_display_timed_out_ = false;

 private:
  // Update |icon| to represent the current state of the auth factor. Called by
  // RefreshUI, so implementations do not need to call this method directly.
  virtual void UpdateIcon(AuthIconView* icon) = 0;

  // This will be called after the latest error has been shown to the user to
  // signal that any transient error state should be cleared out.
  // Implementations should not call `RefreshUI` since that is handled by
  // `HandleErrorTimeout`.
  virtual void DoHandleErrorTimeout() = 0;

  // This will be called when the auth factor's icon is tapped or clicked.
  // Implementations should not call `RefreshUI` since that is handled by
  // `HandleTapOrClick`.
  virtual void DoHandleTapOrClick() = 0;

  base::RepeatingClosure update_state_callback_;
  AuthIconView* icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_
