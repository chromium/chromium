// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_DIALOG_WINDOW_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_DIALOG_WINDOW_WAITER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Waits for a dialog window to open and become visible.
//
// Starts listening for window creation events on construction. `Wait` blocks
// until the expected dialog window is visible. `Wait` returns immediately if
// the expected dialog window is already visible when `Wait` is called.
//
// DialogWindowWaiter is single-use. It can only wait for one dialog to be
// opened per lifetime.
class DialogWindowWaiter : public aura::EnvObserver,
                           public aura::WindowObserver {
 public:
  // Starts listening for a dialog window to open with title `dialog_title`.
  explicit DialogWindowWaiter(const std::u16string& dialog_title);

  DialogWindowWaiter(const DialogWindowWaiter&) = delete;
  DialogWindowWaiter& operator=(const DialogWindowWaiter&) = delete;

  ~DialogWindowWaiter() override;

  // Blocks until a dialog with title `dialog_title` becomes visible. All calls
  // to `Wait` return immediately after the dialog becomes visible during this
  // object's lifetime.
  void Wait();

  // aura::EnvObserver
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  // The title of the expected dialog window.
  std::u16string dialog_title_;

  base::RunLoop run_loop_;

  std::set<raw_ptr<aura::Window, SetExperimental>> dialog_windows_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_DIALOG_WINDOW_WAITER_H_
