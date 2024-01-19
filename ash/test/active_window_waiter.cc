// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/active_window_waiter.h"

#include "ash/shell.h"
#include "base/run_loop.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

ActiveWindowWaiter::ActiveWindowWaiter(aura::Window* root_window) {
  observation_.Observe(wm::GetActivationClient(root_window));
}

ActiveWindowWaiter::~ActiveWindowWaiter() = default;

aura::Window* ActiveWindowWaiter::Wait() {
  run_loop_.Run();
  return found_window_;
}

void ActiveWindowWaiter::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active) {
    found_window_ = gained_active;
    observation_.Reset();
    run_loop_.Quit();
  }
}

}  // namespace ash
