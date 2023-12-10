// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_H_
#define ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

class UserContext;

using LocalAuthenticationCallback =
    base::OnceCallback<void(bool success, std::unique_ptr<UserContext>)>;

// LocalAuthenticationRequestController serves local authentication requests
// regarding the re-auth session. It takes care of showing and hiding the UI.
class ASH_PUBLIC_EXPORT LocalAuthenticationRequestController {
 public:
  static LocalAuthenticationRequestController* Get();

  virtual ~LocalAuthenticationRequestController();

  // Shows a standalone local authentication dialog.
  // |callback| is invoked when the widget is closed e.g with the back button
  // or the correct code is entered.
  // Returns whether opening the dialog was successful. Will fail if another
  // dialog is already opened.
  virtual bool ShowWidget(LocalAuthenticationCallback callback,
                          std::unique_ptr<UserContext> user_context) = 0;

 protected:
  LocalAuthenticationRequestController();
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_H_
