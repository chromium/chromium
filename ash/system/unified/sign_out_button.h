// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_SIGN_OUT_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_SIGN_OUT_BUTTON_H_

#include "ash/system/unified/rounded_label_button.h"
#include "base/macros.h"

namespace views {
class ButtonListener;
}  // namespace views

namespace ash {

// Sign out button to be shown in TopShortcutView with TopShortcutButtons.
// Shows the label like "Sign out", "Exit guest", etc. depending on the session
// status.
class SignOutButton : public RoundedLabelButton {
 public:
  explicit SignOutButton(views::ButtonListener* listener);
  ~SignOutButton() override;

  // views::RoundedLabelButton:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignOutButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_SIGN_OUT_BUTTON_H_
