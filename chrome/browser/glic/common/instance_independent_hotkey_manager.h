// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_INSTANCE_INDEPENDENT_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_GLIC_COMMON_INSTANCE_INDEPENDENT_HOTKEY_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"

namespace glic {

class GlicInstanceCoordinatorImpl;

// Manages instance-independent hotkeys that are active application-wide.
// These hotkeys are registered globally across all browser windows and are
// handled by the GlicInstanceCoordinator. They do not depend on a specific
// embedder panel being active or focused.
// Typically used for global triggers like toggling the active Glic instance,
// activating the browser window, etc.
class InstanceIndependentHotkeyManager
    : public LocalHotkeyManager::EventHandler {
 public:
  explicit InstanceIndependentHotkeyManager(
      GlicInstanceCoordinatorImpl* coordinator);
  ~InstanceIndependentHotkeyManager() override;

  // LocalHotkeyManager::EventHandler:
  bool AcceleratorPressed(LocalHotkeyManager::Command command) override;
  bool CanHandleAccelerators() const override;

 private:
  std::unique_ptr<LocalHotkeyManager> hotkey_manager_;
  raw_ptr<GlicInstanceCoordinatorImpl> coordinator_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_INSTANCE_INDEPENDENT_HOTKEY_MANAGER_H_
