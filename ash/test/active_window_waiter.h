// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ACTIVE_WINDOW_WAITER_H_
#define ASH_TEST_ACTIVE_WINDOW_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace aura {
class Window;
}

namespace ash {

// Waits for a window to be activated on a given root window. Returns the
// activated window via the Wait() method.
class ActiveWindowWaiter : public wm::ActivationChangeObserver {
 public:
  explicit ActiveWindowWaiter(aura::Window* root_window);
  ActiveWindowWaiter(const ActiveWindowWaiter&) = delete;
  ActiveWindowWaiter& operator=(const ActiveWindowWaiter&) = delete;
  ~ActiveWindowWaiter() override;

  // Returns the activated window.
  aura::Window* Wait();

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  base::RunLoop run_loop_;
  raw_ptr<aura::Window> found_window_ = nullptr;
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_TEST_ACTIVE_WINDOW_WAITER_H_
