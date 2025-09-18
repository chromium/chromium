// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_
#define ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

class AccountId;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ASH_PUBLIC_EXPORT MultiUserWindowManagerObserver
    : public base::CheckedObserver {
 public:
  // Called when the owner of a window supplied to SetWindowOwner() changes.
  // |was_minimized| is true if the window was minimized. |teleported| is true
  // if the window was not on the desktop of the current user.
  virtual void OnWindowOwnerEntryChanged(aura::Window* window,
                                         const AccountId& account_id,
                                         bool was_minimized,
                                         bool teleported) {}

  // Called at the time when the user's shelf should transition to the account
  // supplied to OnWillSwitchActiveAccount().
  virtual void OnTransitionUserShelfToNewAccount() {}

 protected:
  ~MultiUserWindowManagerObserver() override = default;
};

}  // namespace ash

#endif  // ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_
