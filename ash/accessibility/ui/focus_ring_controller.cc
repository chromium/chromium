// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/focus_ring_controller.h"

#include <memory>
#include <string_view>

#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/accessibility/ui/native_focus_watcher.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

FocusRingController::FocusRingController() = default;

FocusRingController::~FocusRingController() {
  SetVisible(false);
  if (native_focus_watcher_) {
    native_focus_watcher_->RemoveObserver(this);
    native_focus_watcher_.reset();
  }
}

void FocusRingController::OnNativeFocusChanged(
    const gfx::Rect& bounds_in_screen) {
  bounds_in_screen_ = bounds_in_screen;
  UpdateFocusRing();
}
void FocusRingController::OnNativeFocusCleared() {
  ClearFocusRing();
}

void FocusRingController::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;

  if (visible_) {
    if (!native_focus_watcher_) {
      native_focus_watcher_ = std::make_unique<NativeFocusWatcher>();
      native_focus_watcher_->AddObserver(this);
    }
    native_focus_watcher_->SetEnabled(true);
  } else if (native_focus_watcher_) {
    // If !visible_, disable watching focus.
    native_focus_watcher_->SetEnabled(false);
  }
}

void FocusRingController::UpdateFocusRing() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window) {
    return;
  }
  aura::Window* root_window = active_window->GetRootWindow();
  if (!focus_ring_layer_) {
    focus_ring_layer_ = std::make_unique<FocusRingLayer>(this);
  }
  aura::Window* container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);
  focus_ring_layer_->Set(container, bounds_in_screen_, /*stack_at_top=*/true);
}

void FocusRingController::ClearFocusRing() {
  focus_ring_layer_.reset();
}

void FocusRingController::OnDeviceScaleFactorChanged() {
  UpdateFocusRing();
}
}  // namespace ash
