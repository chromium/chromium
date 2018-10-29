// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardController;
}

namespace ash {

class RootWindowController;
class SessionController;
class VirtualKeyboardController;

// Contains and observes a keyboard::KeyboardController instance. Ash specific
// behavior, including implementing the mojo interface, is implemented in this
// class. TODO(stevenjb): Consider re-factoring keyboard::KeyboardController so
// that this can inherit from that class instead.
class ASH_EXPORT AshKeyboardController
    : public mojom::KeyboardController,
      public keyboard::KeyboardControllerObserver,
      public SessionObserver {
 public:
  // |session_controller| is expected to outlive AshKeyboardController.
  explicit AshKeyboardController(SessionController* session_controller);
  ~AshKeyboardController() override;

  void BindRequest(mojom::KeyboardControllerRequest request);

  // Enables the keyboard controller if enabling has been requested. If already
  // enabled, the keyboard is disabled and re-enabled.
  void EnableKeyboard();

  // Disables the keyboard.
  void DisableKeyboard();

  // Create or destroy the virtual keyboard. Called from Shell. TODO(stevenjb):
  // Fix dependencies so that the virtual keyboard can be created with the
  // keyboard controller.
  void CreateVirtualKeyboard();
  void DestroyVirtualKeyboard();

  // mojom::KeyboardController:
  void GetKeyboardConfig(GetKeyboardConfigCallback callback) override;
  void SetKeyboardConfig(
      keyboard::mojom::KeyboardConfigPtr keyboard_config) override;
  void IsKeyboardEnabled(IsKeyboardEnabledCallback callback) override;
  void SetEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override;
  void ClearEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override;
  void ReloadKeyboard() override;
  void AddObserver(
      mojom::KeyboardControllerObserverAssociatedPtrInfo observer) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  // Activates the keyboard controller for the primary root window controller.
  void ActivateKeyboard();

  // Activates the keyboard controller for |controller|.
  void ActivateKeyboardForRoot(RootWindowController* controller);

  // Deactivates the keyboard controller.
  void DeactivateKeyboard();

 private:
  // Called whenever the enable flags may have changed the enabled state from
  // |was_enabled|. If changed, enables or disables the keyboard.
  void UpdateEnableFlag(bool was_enabled);

  // keyboard::KeyboardControllerObserver
  void OnKeyboardConfigChanged() override;
  void OnKeyboardVisibilityStateChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  SessionController* session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardController> keyboard_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  mojo::BindingSet<mojom::KeyboardController> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::KeyboardControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardController);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
