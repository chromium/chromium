// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_H_
#define ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"

class AccountId;

namespace aura {
class Window;
}

namespace ash {

class MultiUserWindowManagerDelegate;
class MultiUserWindowManagerObserver;

// Used to assign windows to user accounts so that ash shows the appropriate set
// of windows based on the active user.
class ASH_EXPORT MultiUserWindowManager {
 public:
  static std::unique_ptr<MultiUserWindowManager> Create(
      MultiUserWindowManagerDelegate* delegate,
      const AccountId& account_id);

  virtual ~MultiUserWindowManager() {}

  // Associates a window with a particular account. This may result in hiding
  // |window|. This should *not* be called more than once with a different
  // account. If |window| was created by a user gesture
  // (aura::client::kCreatedByUserGesture), then the 'shown' account is set to
  // the current account.
  virtual void SetWindowOwner(aura::Window* window,
                              const AccountId& account_id) = 0;

  // Shows a previously registered window for the specified account.
  virtual void ShowWindowForUser(aura::Window* window,
                                 const AccountId& account_id) = 0;

  virtual const AccountId& GetWindowOwner(const aura::Window* window) const = 0;

  // Returns true if at least one window's 'owner' account differs from its
  // 'shown' account. In other words, a window from one account is shown with
  // windows from another account.
  virtual bool AreWindowsSharedAmongUsers() const = 0;

  // Returns the set owners for the visible windows.
  virtual std::set<AccountId> GetOwnersOfVisibleWindows() const = 0;

  // Returns the user for which the window is currently shown. An empty
  // AccountId() is returned if the window is presented for every user.
  virtual const AccountId& GetUserPresentingWindow(
      const aura::Window* window) const = 0;

  // Returns the id of the currently active user.
  virtual const AccountId& CurrentAccountId() const = 0;

  virtual void AddObserver(MultiUserWindowManagerObserver* observer) = 0;
  virtual void RemoveObserver(MultiUserWindowManagerObserver* observer) = 0;

 protected:
  MultiUserWindowManager() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_H_
