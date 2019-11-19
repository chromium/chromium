// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/sign_out_button.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/user/login_status.h"

namespace ash {

SignOutButton::SignOutButton(views::ButtonListener* listener)
    : RoundedLabelButton(listener,
                         user::GetLocalizedSignOutStringForStatus(
                             Shell::Get()->session_controller()->login_status(),
                             false /* multiline */)) {}

SignOutButton::~SignOutButton() = default;

const char* SignOutButton::GetClassName() const {
  return "SignOutButton";
}

}  // namespace ash
