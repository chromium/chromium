// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"

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

bool TabletMode::IsInTabletMode() {
  const TabletMode* singleton = TabletMode::Get();
  return singleton && singleton->InTabletMode();
}

}  // namespace ash
