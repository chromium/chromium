// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "base/logging.h"

namespace ash {

namespace {
TabletMode* g_instance = nullptr;
}

TabletMode* TabletMode::Get() {
  return g_instance;
}

TabletMode::TabletMode() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

TabletMode::~TabletMode() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

TabletMode::Waiter::Waiter(bool enable)
    : enable_(enable), run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
  if (TabletMode::Get()->InTabletMode() == enable_)
    run_loop_.Quit();
  else
    TabletMode::Get()->AddObserver(this);
}

TabletMode::Waiter::~Waiter() {
  TabletMode::Get()->RemoveObserver(this);
}

void TabletMode::Waiter::Wait() {
  run_loop_.Run();
}

void TabletMode::Waiter::OnTabletModeStarted() {
  if (enable_)
    run_loop_.QuitWhenIdle();
}

void TabletMode::Waiter::OnTabletModeEnded() {
  if (!enable_)
    run_loop_.QuitWhenIdle();
}

}  // namespace ash
