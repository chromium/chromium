// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_

#include <string>

namespace ash {

class AuthIconView;

enum class AuthFactorType {
  kFingerprint = 1 << 0,
  kSmartLock = 1 << 1,
};

// Allow combining AuthFactorTypes with bitwise OR. Used to look up the
// appropriate label to show when several auth factors are visible.
int operator|(int types, AuthFactorType type);
int operator|(AuthFactorType type1, AuthFactorType type2);

// Base class representing an auth factor. Used by LoginAuthFactorsView to
// display a list of auth factors.
class AuthFactorModel {
 public:
  enum class AuthFactorState {
    // The feature is disabled, disallowed by policy, or requires
    // hardware that isn’t present.
    kUnavailable,
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
    // The auth factor cannot be used because of an unrecoverable
    // error, e.g. Fingerprint’s “Too many attempts”. GetLabel()
    // and UpdateIcon() show the relevant messages.
    kErrorPermanent,
    // The auth factor requires the user to tap/click to enter.
    kClickRequired,
    // Authentication is complete.
    kAuthenticated,
  };

  AuthFactorModel();
  AuthFactorModel(AuthFactorModel&) = delete;
  AuthFactorModel& operator=(AuthFactorModel&) = delete;
  virtual ~AuthFactorModel();

  virtual AuthFactorState GetAuthFactorState() = 0;

  virtual AuthFactorType GetType() = 0;

  // The label that should be shown in the current state.
  virtual std::u16string GetLabel() = 0;

  // Controls whether the label is announced by Chromevox.
  virtual bool ShouldAnnounceLabel() = 0;

  // Update an AuthIconView to represent the current state of the auth factor.
  // Should call SetIcon() or set up an animation.
  virtual void UpdateIcon(AuthIconView* icon_view) = 0;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_FACTOR_MODEL_H_
