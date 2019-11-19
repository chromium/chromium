// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_controller_impl.h"

#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

using keyboard::KeyboardConfig;
using keyboard::KeyboardEnableFlag;

namespace ash {

namespace {

base::Optional<display::Display> GetFirstTouchDisplay() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE)
      return display;
  }
  return base::nullopt;
}

}  // namespace

KeyboardControllerImpl::KeyboardControllerImpl(
    SessionControllerImpl* session_controller)
    : session_controller_(session_controller),
      keyboard_ui_controller_(
          std::make_unique<keyboard::KeyboardUIController>()) {
  if (session_controller_)  // May be null in tests.
    session_controller_->AddObserver(this);
  keyboard_ui_controller_->AddObserver(this);
}

KeyboardControllerImpl::~KeyboardControllerImpl() {
  keyboard_ui_controller_->RemoveObserver(this);
  if (session_controller_)  // May be null in tests.
    session_controller_->RemoveObserver(this);
}

void KeyboardControllerImpl::CreateVirtualKeyboard(
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory) {
  DCHECK(keyboard_ui_factory);
  virtual_keyboard_controller_ = std::make_unique<VirtualKeyboardController>();
  keyboard_ui_controller_->Initialize(std::move(keyboard_ui_factory), this);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard)) {
    keyboard_ui_controller_->SetEnableFlag(
        KeyboardEnableFlag::kCommandLineEnabled);
  }
}

void KeyboardControllerImpl::DestroyVirtualKeyboard() {
  virtual_keyboard_controller_.reset();
  keyboard_ui_controller_->Shutdown();
}

void KeyboardControllerImpl::SendOnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardVisibleBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardVisibleBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::SendOnKeyboardUIDestroyed() {
  for (auto& observer : observers_)
    observer.OnKeyboardUIDestroyed();
}

// ash::KeyboardController

keyboard::KeyboardConfig KeyboardControllerImpl::GetKeyboardConfig() {
  return keyboard_ui_controller_->keyboard_config();
}

void KeyboardControllerImpl::SetKeyboardConfig(
    const KeyboardConfig& keyboard_config) {
  keyboard_ui_controller_->UpdateKeyboardConfig(keyboard_config);
}

bool KeyboardControllerImpl::IsKeyboardEnabled() {
  return keyboard_ui_controller_->IsEnabled();
}

void KeyboardControllerImpl::SetEnableFlag(KeyboardEnableFlag flag) {
  keyboard_ui_controller_->SetEnableFlag(flag);
}

void KeyboardControllerImpl::ClearEnableFlag(KeyboardEnableFlag flag) {
  keyboard_ui_controller_->ClearEnableFlag(flag);
}

const std::set<keyboard::KeyboardEnableFlag>&
KeyboardControllerImpl::GetEnableFlags() {
  return keyboard_ui_controller_->keyboard_enable_flags();
}

void KeyboardControllerImpl::ReloadKeyboardIfNeeded() {
  keyboard_ui_controller_->Reload();
}

void KeyboardControllerImpl::RebuildKeyboardIfEnabled() {
  // Test IsKeyboardEnableRequested in case of an unlikely edge case where this
  // is called while after the enable state changed to disabled (in which case
  // we do not want to override the requested state).
  keyboard_ui_controller_->RebuildKeyboardIfEnabled();
}

bool KeyboardControllerImpl::IsKeyboardVisible() {
  return keyboard_ui_controller_->IsKeyboardVisible();
}

void KeyboardControllerImpl::ShowKeyboard() {
  if (keyboard_ui_controller_->IsEnabled())
    keyboard_ui_controller_->ShowKeyboard(false /* lock */);
}

void KeyboardControllerImpl::HideKeyboard(HideReason reason) {
  if (!keyboard_ui_controller_->IsEnabled())
    return;
  switch (reason) {
    case HideReason::kUser:
      keyboard_ui_controller_->HideKeyboardByUser();
      break;
    case HideReason::kSystem:
      keyboard_ui_controller_->HideKeyboardExplicitlyBySystem();
      break;
  }
}

void KeyboardControllerImpl::SetContainerType(
    keyboard::ContainerType container_type,
    const base::Optional<gfx::Rect>& target_bounds,
    SetContainerTypeCallback callback) {
  keyboard_ui_controller_->SetContainerType(container_type, target_bounds,
                                            std::move(callback));
}

void KeyboardControllerImpl::SetKeyboardLocked(bool locked) {
  keyboard_ui_controller_->set_keyboard_locked(locked);
}

void KeyboardControllerImpl::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  // TODO(https://crbug.com/826617): Support occluded bounds with multiple
  // rectangles.
  keyboard_ui_controller_->SetOccludedBounds(bounds.empty() ? gfx::Rect()
                                                            : bounds[0]);
}

void KeyboardControllerImpl::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_ui_controller_->SetHitTestBounds(bounds);
}

bool KeyboardControllerImpl::SetAreaToRemainOnScreen(const gfx::Rect& bounds) {
  return keyboard_ui_controller_->SetAreaToRemainOnScreen(bounds);
}

void KeyboardControllerImpl::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_ui_controller_->SetDraggableArea(bounds);
}

void KeyboardControllerImpl::AddObserver(KeyboardControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void KeyboardControllerImpl::RemoveObserver(
    KeyboardControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

// SessionObserver
void KeyboardControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!keyboard_ui_controller_->IsEnabled())
    return;

  switch (state) {
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      // Reload the keyboard on user profile change to refresh keyboard
      // extensions with the new profile and ensure the extensions call the
      // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
      // keyboard works on supervised user creation, http://crbug.com/712873.
      // |ACTIVE| is also needed for guest user workflow.
      RebuildKeyboardIfEnabled();
      break;
    default:
      break;
  }
}

void KeyboardControllerImpl::OnRootWindowClosing(aura::Window* root_window) {
  if (keyboard_ui_controller_->GetRootWindow() == root_window) {
    aura::Window* new_parent = GetContainerForDefaultDisplay();
    DCHECK_NE(root_window, new_parent);
    keyboard_ui_controller_->MoveToParentContainer(new_parent);
  }
}

aura::Window* KeyboardControllerImpl::GetContainerForDisplay(
    const display::Display& display) {
  DCHECK(display.is_valid());

  RootWindowController* controller =
      Shell::Get()->GetRootWindowControllerWithDisplayId(display.id());
  aura::Window* container =
      controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
  DCHECK(container);
  return container;
}

aura::Window* KeyboardControllerImpl::GetContainerForDefaultDisplay() {
  const display::Screen* screen = display::Screen::GetScreen();
  const base::Optional<display::Display> first_touch_display =
      GetFirstTouchDisplay();
  const bool has_touch_display = first_touch_display.has_value();

  if (window_util::GetFocusedWindow()) {
    // Return the focused display if that display has touch capability or no
    // other display has touch capability.
    const display::Display focused_display =
        screen->GetDisplayNearestWindow(window_util::GetFocusedWindow());
    if (focused_display.is_valid() &&
        (focused_display.touch_support() ==
             display::Display::TouchSupport::AVAILABLE ||
         !has_touch_display)) {
      return GetContainerForDisplay(focused_display);
    }
  }

  // Return the first touch display, or the primary display if there are none.
  return GetContainerForDisplay(
      has_touch_display ? *first_touch_display : screen->GetPrimaryDisplay());
}

void KeyboardControllerImpl::OnKeyboardConfigChanged(
    const keyboard::KeyboardConfig& config) {
  for (auto& observer : observers_)
    observer.OnKeyboardConfigChanged(config);
}

void KeyboardControllerImpl::OnKeyboardVisibilityChanged(bool is_visible) {
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(is_visible);
}

void KeyboardControllerImpl::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  SendOnKeyboardVisibleBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardOccludedBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnableFlagsChanged(flags);
}

void KeyboardControllerImpl::OnKeyboardEnabledChanged(bool is_enabled) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnabledChanged(is_enabled);
}

}  // namespace ash
