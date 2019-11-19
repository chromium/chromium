// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_WINDOW_VISIBILITY_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_WINDOW_VISIBILITY_WAITER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace chromeos {

// Waits for the window that hosts OOBE UI changes visibility to target value.
// When waiting for the OOBE UI window to be hidden, it handles the window
// getting destroyed. Window getting destroyed while waiting for the window
// to become visible will stop the waiter, but will cause a test failure.
class OobeWindowVisibilityWaiter : public aura::WindowObserver {
 public:
  explicit OobeWindowVisibilityWaiter(bool target_visibilty);
  ~OobeWindowVisibilityWaiter() override;

  void Wait();

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  aura::Window* GetWindow();

  const bool target_visibility_;
  base::OnceClosure wait_stop_closure_;
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(OobeWindowVisibilityWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_WINDOW_VISIBILITY_WAITER_H_
