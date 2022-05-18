// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_

#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ui/views/widget/widget.h"

namespace ash {

class InSessionAuthDialogControllerImpl : public InSessionAuthDialogController {
 public:
  InSessionAuthDialogControllerImpl() = default;
  InSessionAuthDialogControllerImpl(const InSessionAuthDialogControllerImpl&) =
      delete;
  InSessionAuthDialogControllerImpl& operator=(
      const InSessionAuthDialogControllerImpl&) = delete;
  ~InSessionAuthDialogControllerImpl() override = default;

  // InSessionAuthDialogController overrides
  void ShowAuthDialog(Reason reason, OnAuthComplete on_auth_complete) override;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_
