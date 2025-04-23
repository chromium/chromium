// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FOCUS_SHUTDOWN_FOCUS_RULES_H_
#define ASH_FOCUS_SHUTDOWN_FOCUS_RULES_H_

#include "ash/ash_export.h"
#include "ash/focus/ash_focus_rules.h"

namespace ash {

// Focus rules used during shutdown. Nothing is activatable or focusable.
class ASH_EXPORT ShutdownFocusRules : public AshFocusRules {
 public:
  ShutdownFocusRules();

  ShutdownFocusRules(const ShutdownFocusRules&) = delete;
  ShutdownFocusRules& operator=(const ShutdownFocusRules&) = delete;

  ~ShutdownFocusRules() override;

  // AshFocusRules:
  bool SupportsChildActivation(const aura::Window* window) const override;
  bool CanActivateWindow(const aura::Window* window) const override;
  bool CanFocusWindow(const aura::Window* window,
                      const ui::Event* event) const override;
  aura::Window* GetNextActivatableWindow(aura::Window* ignore) const override;
};

}  // namespace ash

#endif  // ASH_FOCUS_SHUTDOWN_FOCUS_RULES_H_
