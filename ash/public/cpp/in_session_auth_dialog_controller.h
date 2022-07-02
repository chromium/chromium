// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// InSessionAuthDialogController manages the in session auth dialog.
class ASH_PUBLIC_EXPORT InSessionAuthDialogController {
 public:
  enum Reason {
    kAccessPasswordManager,
    kModifyAuthFactors,
    kModifyAuthFactorsMultidevice
  };

  // Callback passed from clients of the dialog
  // `success`: Whether or not the authentication was successful.
  // `token`: If the authentication was successful, a token is returned from
  // backends
  //   that can be passed to further sensitive operations (such as those in
  //   quickUnlockPrivate).
  // `timeout`: The length of time for which the token is valid.
  using OnAuthComplete =
      base::OnceCallback<void(bool success,
                              const base::UnguessableToken& token,
                              base::TimeDelta timeout)>;

  InSessionAuthDialogController() = default;
  virtual ~InSessionAuthDialogController() = default;

  // Summons a native UI dialog that authenticates the user, providing a
  // token, timeout and status in return.
  // `reason`: Indicates security context.
  // `prompt`: UI customization, the string shown to the user (e.g, in
  // the context of password manager: "please authenticate to see
  // saved passwords").
  virtual void ShowAuthDialog(Reason reason,
                              OnAuthComplete on_auth_complete) = 0;

  // Must be called with a non null auth_token_provider prior to calling
  // `ShowAuthDialog`.
  // Injects a specific implementation of `InSessionAuthTokenProvider`
  // for generating an `AuthToken` after successful authentication.
  virtual void SetTokenProvider(
      InSessionAuthTokenProvider* auth_token_provider) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
