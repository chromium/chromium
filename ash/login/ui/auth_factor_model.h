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
  // DO NOT change the relative ordering of these enum values. The values
  // assigned here correspond to the priority of these states. For example, if
  // LoginAuthFactorsView has one auth factor in the kClickRequired state and
  // one auth factor in the kReady state, then it will prioritize showing the
  // kClickRequired state since it's assigned a higher priority. With the
  // exception of the error states, a higher priority generally implies that
  // there are fewer steps left to complete authentication.
  enum class AuthFactorState {
    // The feature is disabled, disallowed by policy, or requires
    // hardware that isn’t present.
    kUnavailable = 0,
    // The auth factor cannot be used because of an unrecoverable
    // error, e.g. Fingerprint’s “Too many attempts”. GetLabel()
    // and UpdateIcon() show the relevant messages.
    kErrorPermanent = 1,
    // The auth factor can be used but requires additional steps
    // before use, e.g. turn on Bluetooth.
    kAvailable = 2,
    // The auth factor is ready to authenticate. This state should
    // only be returned if authentication can be completed in one
    // step (two if a click is required).
    kReady = 3,
    // The auth factor has a non-blocking error to show the
    // user, e.g. Fingerprint’s “Not recognized”, which clears
    // after a few seconds. GetLabel() and UpdateIcon() show the
    // relevant messages.
    kErrorTemporary = 4,
    // The auth factor requires the user to tap/click to enter.
    kClickRequired = 5,
    // Authentication is complete.
    kAuthenticated = 6,
  };

  AuthFactorModel();
  AuthFactorModel(AuthFactorModel&) = delete;
  AuthFactorModel& operator=(AuthFactorModel&) = delete;
  virtual ~AuthFactorModel();

  // Initializes |icon_| and |on_state_changed_callback_|. Should be called
  // exactly once before any other methods. The |on_state_changed_callback| is
  // used by LoginAuthFactorsView to determine when it is necessary to update
  // the displayed icons and label.
  // TODO(crbug.com/1233614): Refactor to disallow the creation of partially
  // initialized objects.
  void Init(AuthIconView* icon,
            base::RepeatingClosure on_state_changed_callback);

  // Set the visibility of the associated icon.
  void SetVisible(bool visible);

  // Should be called when the parent View's theme changes.
  void OnThemeChanged();

  // Return the current state of this auth factor.
  virtual AuthFactorState GetAuthFactorState() = 0;

  // Returns the type of the auth factor. Each implementation of AuthFactorModel
  // should add a new type to the AuthFactorType enum.
  virtual AuthFactorType GetType() = 0;

  // The ID of the label that should be shown in the current state.
  virtual int GetLabelId() = 0;

  // Controls whether the label is announced by Chromevox.
  virtual bool ShouldAnnounceLabel() = 0;

  // Alternative text to be provided to screen readers.
  virtual int GetAccessibleNameId() = 0;

  // This will be called when the auth factor's icon is tapped or clicked.
  virtual void OnTapOrClickEvent() = 0;

 protected:
  // Should be called whenever the internal state of the auth model changes to
  // invoke the |on_state_changed_callback_| if set. Calls UpdateIcon().
  void NotifyOnStateChanged();

 private:
  // Update |icon| to represent the current state of the auth factor. Called by
  // NotifyOnStateChanged(), so implementations do not need to call this
  // method directly.
  virtual void UpdateIcon(AuthIconView* icon) = 0;

  base::RepeatingClosure on_state_changed_callback_;
  AuthIconView* icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_
