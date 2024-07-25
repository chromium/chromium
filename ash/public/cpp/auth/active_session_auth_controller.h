// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// ActiveSessionAuthController serves active session authentication requests.
// It takes care of showing and hiding the UI and the authentication process.
class ASH_PUBLIC_EXPORT ActiveSessionAuthController {
 public:
  using AuthCompletionCallback =
      base::OnceCallback<void(bool success,
                              const ash::AuthProofToken& token,
                              base::TimeDelta timeout)>;
  enum Reason {
    kPasswordManager,
    kSettings,
  };

  static ActiveSessionAuthController* Get();

  virtual ~ActiveSessionAuthController();

  // Shows a standalone authentication widget.
  // |callback| is invoked when the widget is closed e.g with the back button
  // or the correct code is entered.
  // Returns whether opening the widget was successful. Will fail if another
  // widget is already opened.
  virtual bool ShowAuthDialog(Reason reason,
                              const AccountId& account_id,
                              AuthCompletionCallback on_auth_complete) = 0;

  virtual bool IsShown() const = 0;

 protected:
  ActiveSessionAuthController();
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_
