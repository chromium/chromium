// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/pre_target_accelerator_handler.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/system_shortcut_behavior_policy.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/wm/core/window_util.h"

namespace ash {

PreTargetAcceleratorHandler::PreTargetAcceleratorHandler() = default;

PreTargetAcceleratorHandler::~PreTargetAcceleratorHandler() = default;

bool PreTargetAcceleratorHandler::ProcessAccelerator(
    const ui::KeyEvent& key_event,
    const ui::Accelerator& accelerator) {
  aura::Window* target = static_cast<aura::Window*>(key_event.target());
  // Callers should never supply null.
  DCHECK(target);
  // Special hardware keys like brightness and volume are handled in
  // special way. However, some windows can override this behavior
  // (e.g. Chrome v1 apps by default and Chrome v2 apps with
  // permission) by setting a window property.
  if (AcceleratorController::IsSystemKey(key_event.key_code()) &&
      !CanConsumeSystemKeys(target, key_event)) {
    // System keys are always consumed regardless of whether they trigger an
    // accelerator to prevent windows from seeing unexpected key up events.
    Shell::Get()->accelerator_controller()->Process(accelerator);
    return true;
  }
  if (!ShouldProcessAcceleratorNow(target, key_event, accelerator))
    return false;
  return Shell::Get()->accelerator_controller()->Process(accelerator);
}

bool PreTargetAcceleratorHandler::CanConsumeSystemKeys(
    aura::Window* target,
    const ui::KeyEvent& event) {
  // Uses the top level window so if the target is a web contents window the
  // containing parent window will be checked for the property.
  aura::Window* top_level = ::wm::GetToplevelWindow(target);
  return top_level && WindowState::Get(top_level)->CanConsumeSystemKeys();
}

bool PreTargetAcceleratorHandler::ShouldProcessAcceleratorNow(
    aura::Window* target,
    const ui::KeyEvent& event,
    const ui::Accelerator& accelerator) {
  const aura::Window* top_level = ::wm::GetToplevelWindow(target);
  const bool is_fullscreen =
      top_level && WindowState::Get(top_level)->IsFullscreen();

  // Callers should never supply null.
  DCHECK(target);
  // On ChromeOS, If the accelerator is Search+<key(s)> then it must never be
  // intercepted by apps or windows.
  if (accelerator.IsCmdDown() || accelerator.key_code() == ui::VKEY_LWIN ||
      accelerator.key_code() == ui::VKEY_RWIN) {
    const auto system_shortcut_behavior_policy = GetSystemShortcutBehavior();
    switch (system_shortcut_behavior_policy) {
      case SystemShortcutBehaviorType::kNormalShortcutBehavior:
      case SystemShortcutBehaviorType::kIgnoreCommonVdiShortcuts:
      case SystemShortcutBehaviorType::kIgnoreCommonVdiShortcutsFullscreenOnly:
        return true;

      // Always allow shortcuts with search down to passthrough.
      case SystemShortcutBehaviorType::kAllowSearchBasedPassthrough:
        break;

      // Only allow to passthrough if the target app is fullscreen.
      case SystemShortcutBehaviorType::
          kAllowSearchBasedPassthroughFullscreenOnly:
        if (!is_fullscreen) {
          return true;
        }
        break;
    }
  }

  if (base::Contains(Shell::GetAllRootWindows(), target))
    return true;

  AcceleratorControllerImpl* accelerator_controller =
      Shell::Get()->accelerator_controller();

  // Reserved accelerators (such as Power button) always have a priority.
  if (accelerator_controller->IsReserved(accelerator))
    return true;

  // A full screen window has a right to handle all key events including the
  // reserved ones.
  if (is_fullscreen) {
    // On ChromeOS, fullscreen windows are either browser or apps, which
    // send key events to a web content first, then will process keys
    // if the web content didn't consume them.
    return false;
  }

  // Handle preferred accelerators (such as ALT-TAB) before sending
  // to the target.
  return accelerator_controller->IsPreferred(accelerator);
}

}  // namespace ash
