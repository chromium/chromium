// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_
#define ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

class AccountId;

namespace aura {
class Window;
}

namespace ash {

class ASH_PUBLIC_EXPORT MultiUserWindowManagerDelegate {
 public:
  // Called when the owner of a window supplied to SetWindowOwner() changes.
  // |was_minimized| is true if the window was minimized. |teleported| is true
  // if the window was not on the desktop of the current user.
  virtual void OnWindowOwnerEntryChanged(aura::Window* window,
                                         const AccountId& account_id,
                                         bool was_minimized,
                                         bool teleported) = 0;

  // Called at the time when the user's shelf should transition to the account
  // supplied to OnWillSwitchActiveAccount().
  virtual void OnTransitionUserShelfToNewAccount() = 0;

 protected:
  virtual ~MultiUserWindowManagerDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_
