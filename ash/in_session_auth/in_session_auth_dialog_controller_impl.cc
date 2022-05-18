// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/in_session_auth/authentication_dialog.h"

namespace ash {

void InSessionAuthDialogControllerImpl::ShowAuthDialog(
    Reason reason,
    OnAuthComplete on_auth_complete) {
  // We don't manage the lifetime of `AuthenticationDialog` here.
  // `AuthenticatonDialog` is-a View and it is instead owned by it's widget,
  // which would properly delete it when the widget is closed.
  (new AuthenticationDialog(std::move(on_auth_complete)))->Show();
}

}  // namespace ash
