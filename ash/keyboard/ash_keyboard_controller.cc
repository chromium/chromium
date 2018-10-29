// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_controller.h"

#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"

using keyboard::mojom::KeyboardConfig;
using keyboard::mojom::KeyboardConfigPtr;
using keyboard::mojom::KeyboardEnableFlag;

namespace ash {

AshKeyboardController::AshKeyboardController(
    SessionController* session_controller)
    : session_controller_(session_controller),
      keyboard_controller_(std::make_unique<keyboard::KeyboardController>()) {
  if (session_controller_)  // May be null in tests.
    session_controller_->AddObserver(this);
  keyboard_controller_->AddObserver(this);
}

AshKeyboardController::~AshKeyboardController() {
  keyboard_controller_->RemoveObserver(this);
  if (session_controller_)  // May be null in tests.
    session_controller_->RemoveObserver(this);
}

void AshKeyboardController::BindRequest(
    mojom::KeyboardControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AshKeyboardController::EnableKeyboard() {
  if (!keyboard_controller_->IsKeyboardEnableRequested())
    return;

  // De-activate the keyboard, as some callers expect the keyboard to be
  // reloaded. TODO(https://crbug.com/731537): Add a separate function for
  // reloading the keyboard.
  DeactivateKeyboard();

  // TODO(crbug.com/646565): The keyboard UI uses a WebContents that is
  // created by chrome code but parented to an ash-created container window.
  // See ChromeKeyboardUI and keyboard::KeyboardController. This needs to be
  // fixed for both SingleProcessMash and MultiProcessMash.
  if (::features::IsUsingWindowService())
    return;

  std::unique_ptr<keyboard::KeyboardUI> keyboard_ui =
      Shell::Get()->shell_delegate()->CreateKeyboardUI();
  DCHECK(keyboard_ui);
  keyboard_controller_->EnableKeyboard(std::move(keyboard_ui),
                                       virtual_keyboard_controller_.get());
  ActivateKeyboard();
}

void AshKeyboardController::DisableKeyboard() {
  DeactivateKeyboard();
  keyboard_controller_->DisableKeyboard();
}

void AshKeyboardController::CreateVirtualKeyboard() {
  virtual_keyboard_controller_ = std::make_unique<VirtualKeyboardController>();
}

void AshKeyboardController::DestroyVirtualKeyboard() {
  virtual_keyboard_controller_.reset();
}

void AshKeyboardController::AddObserver(
    mojom::KeyboardControllerObserverAssociatedPtrInfo observer) {
  mojom::KeyboardControllerObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observers_.AddPtr(std::move(observer_ptr));
}

void AshKeyboardController::GetKeyboardConfig(
    GetKeyboardConfigCallback callback) {
  std::move(callback).Run(
      KeyboardConfig::New(keyboard_controller_->keyboard_config()));
}

void AshKeyboardController::SetKeyboardConfig(
    KeyboardConfigPtr keyboard_config) {
  keyboard_controller_->UpdateKeyboardConfig(*keyboard_config);
}

void AshKeyboardController::IsKeyboardEnabled(
    IsKeyboardEnabledCallback callback) {
  std::move(callback).Run(keyboard_controller_->IsEnabled());
}

void AshKeyboardController::SetEnableFlag(KeyboardEnableFlag flag) {
  bool was_enabled = keyboard_controller_->IsEnabled();
  keyboard_controller_->SetEnableFlag(flag);
  UpdateEnableFlag(was_enabled);
}

void AshKeyboardController::ClearEnableFlag(KeyboardEnableFlag flag) {
  bool was_enabled = keyboard_controller_->IsEnabled();
  keyboard_controller_->ClearEnableFlag(flag);
  UpdateEnableFlag(was_enabled);
}

void AshKeyboardController::ReloadKeyboard() {
  // Test IsKeyboardEnableRequested in case of an unlikely edge case where this
  // is called while after the enable state changed to disabled (in which case
  // we do not want to override the requested state).
  if (keyboard_controller_->IsKeyboardEnableRequested())
    EnableKeyboard();
}

void AshKeyboardController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!keyboard_controller_->IsKeyboardEnableRequested())
    return;

  switch (state) {
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
      ActivateKeyboard();
      break;
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      // Reload the keyboard on user profile change to refresh keyboard
      // extensions with the new profile and ensure the extensions call the
      // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
      // keyboard works on supervised user creation, http://crbug.com/712873.
      // |ACTIVE| is also needed for guest user workflow.
      EnableKeyboard();
      break;
    default:
      break;
  }
}

void AshKeyboardController::ActivateKeyboard() {
  ActivateKeyboardForRoot(Shell::Get()->GetPrimaryRootWindowController());
}

void AshKeyboardController::ActivateKeyboardForRoot(
    RootWindowController* controller) {
  DCHECK(controller);
  if (!keyboard_controller_->IsEnabled())
    return;

  // If the keyboard is already activated for |controller|, do nothing.
  if (controller->GetRootWindow() == keyboard_controller_->GetRootWindow())
    return;

  aura::Window* container =
      controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
  DCHECK(container);
  keyboard_controller_->ActivateKeyboardInContainer(container);
  keyboard_controller_->LoadKeyboardWindowInBackground();
}

void AshKeyboardController::DeactivateKeyboard() {
  if (!keyboard_controller_->IsEnabled() ||
      !keyboard_controller_->GetRootWindow()) {
    return;
  }
  keyboard_controller_->DeactivateKeyboard();
}

void AshKeyboardController::UpdateEnableFlag(bool was_enabled) {
  bool is_enabled = keyboard_controller_->IsKeyboardEnableRequested();
  if (is_enabled && !was_enabled) {
    EnableKeyboard();
  } else if (!is_enabled && was_enabled) {
    DisableKeyboard();
  }
}

void AshKeyboardController::OnKeyboardConfigChanged() {
  KeyboardConfigPtr config =
      KeyboardConfig::New(keyboard_controller_->keyboard_config());
  observers_.ForAllPtrs([&config](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardConfigChanged(config.Clone());
  });
}

void AshKeyboardController::OnKeyboardVisibilityStateChanged(bool is_visible) {
  observers_.ForAllPtrs(
      [is_visible](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardVisibilityChanged(is_visible);
      });
}

void AshKeyboardController::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  observers_.ForAllPtrs([&bounds](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardVisibleBoundsChanged(bounds);
  });
}

void AshKeyboardController::OnKeyboardEnabledChanged(bool is_enabled) {
  observers_.ForAllPtrs(
      [is_enabled](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardEnabledChanged(is_enabled);
      });
}

}  // namespace ash
