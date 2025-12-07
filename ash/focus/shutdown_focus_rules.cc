// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus/shutdown_focus_rules.h"

namespace ash {

ShutdownFocusRules::ShutdownFocusRules() = default;
ShutdownFocusRules::~ShutdownFocusRules() = default;

bool ShutdownFocusRules::SupportsChildActivation(
    const aura::Window* window) const {
  return false;
}

bool ShutdownFocusRules::CanActivateWindow(const aura::Window* window) const {
  return false;
}

bool ShutdownFocusRules::CanFocusWindow(const aura::Window* window,
                                        const ui::Event* event) const {
  return false;
}

aura::Window* ShutdownFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  return nullptr;
}

}  // namespace ash
