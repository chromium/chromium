// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_FOCUS_RULES_H_
#define ASH_WM_ASH_FOCUS_RULES_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/wm/core/base_focus_rules.h"

namespace ash {

class ASH_EXPORT AshFocusRules : public ::wm::BaseFocusRules {
 public:
  AshFocusRules();

  AshFocusRules(const AshFocusRules&) = delete;
  AshFocusRules& operator=(const AshFocusRules&) = delete;

  ~AshFocusRules() override;

  // ::wm::BaseFocusRules:
  bool IsToplevelWindow(const aura::Window* window) const override;
  bool SupportsChildActivation(const aura::Window* window) const override;
  bool IsWindowConsideredVisibleForActivation(
      const aura::Window* window) const override;
  bool CanActivateWindow(const aura::Window* window) const override;
  bool CanFocusWindow(const aura::Window* window,
                      const ui::Event* event) const override;
  aura::Window* GetNextActivatableWindow(aura::Window* ignore) const override;

 private:
  aura::Window* GetTopmostWindowToActivateForContainerIndex(
      int index,
      aura::Window* ignore,
      aura::Window* priority_root) const;
  aura::Window* GetTopmostWindowToActivateInContainer(
      aura::Window* container,
      aura::Window* ignore) const;

  // List of container IDs in the order of actiavation. This list doesn't change
  // for the lifetime of this object.
  const std::vector<int> activatable_container_ids_;
};

}  // namespace ash

#endif  // ASH_WM_ASH_FOCUS_RULES_H_
