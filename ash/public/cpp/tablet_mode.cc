// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

namespace {
TabletMode* g_instance = nullptr;
}

// static
bool TabletMode::IsBoardTypeMarkedAsTabletCapable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTabletMode);
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
  if (display::Screen::GetScreen()->InTabletMode() == enable_) {
    run_loop_.Quit();
  } else {
    display::Screen::GetScreen()->AddObserver(this);
  }
}

TabletMode::Waiter::~Waiter() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void TabletMode::Waiter::Wait() {
  run_loop_.Run();
}

void TabletMode::Waiter::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if ((enable_ && state == display::TabletState::kInTabletMode) ||
      (!enable_ && state == display::TabletState::kInClamshellMode)) {
    run_loop_.QuitWhenIdle();
  }
}

}  // namespace ash
