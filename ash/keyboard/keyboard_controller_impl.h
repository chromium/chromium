// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_
#define ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_layout_delegate.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/optional.h"

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardUIController;
class KeyboardUIFactory;
}  // namespace keyboard

namespace ash {

class SessionControllerImpl;
class VirtualKeyboardController;

// Contains and observes a keyboard::KeyboardUIController instance. Ash specific
// behavior, including implementing the public interface, is implemented in this
// class.
class ASH_EXPORT KeyboardControllerImpl
    : public KeyboardController,
      public keyboard::KeyboardLayoutDelegate,
      public KeyboardControllerObserver,
      public SessionObserver {
 public:
  // |session_controller| is expected to outlive KeyboardControllerImpl.
  explicit KeyboardControllerImpl(SessionControllerImpl* session_controller);
  ~KeyboardControllerImpl() override;

  // Create or destroy the virtual keyboard. Called from Shell. TODO(stevenjb):
  // Fix dependencies so that the virtual keyboard can be created with the
  // keyboard controller.
  void CreateVirtualKeyboard(
      std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory);
  void DestroyVirtualKeyboard();

  // Forwards events to observers.
  void SendOnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds);
  void SendOnKeyboardUIDestroyed();

  // ash::KeyboardController:
  keyboard::KeyboardConfig GetKeyboardConfig() override;
  void SetKeyboardConfig(
      const keyboard::KeyboardConfig& keyboard_config) override;
  bool IsKeyboardEnabled() override;
  void SetEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  void ClearEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  const std::set<keyboard::KeyboardEnableFlag>& GetEnableFlags() override;
  void ReloadKeyboardIfNeeded() override;
  void RebuildKeyboardIfEnabled() override;
  bool IsKeyboardVisible() override;
  void ShowKeyboard() override;
  void HideKeyboard(HideReason reason) override;
  void SetContainerType(keyboard::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        SetContainerTypeCallback callback) override;
  void SetKeyboardLocked(bool locked) override;
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;
  void SetDraggableArea(const gfx::Rect& bounds) override;
  void AddObserver(KeyboardControllerObserver* observer) override;
  void RemoveObserver(KeyboardControllerObserver* observer) override;

  // keyboard::KeyboardLayoutDelegate:
  aura::Window* GetContainerForDefaultDisplay() override;
  aura::Window* GetContainerForDisplay(
      const display::Display& display) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard_ui_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  // Called whenever a root window is closing.
  // If the root window contains the virtual keyboard window, deactivates
  // the keyboard so that its window doesn't get destroyed as well.
  void OnRootWindowClosing(aura::Window* root_window);

 private:
  // KeyboardControllerObserver:
  void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) override;
  void OnKeyboardVisibilityChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  SessionControllerImpl* session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardUIController> keyboard_ui_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  base::ObserverList<KeyboardControllerObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerImpl);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_
