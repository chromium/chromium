// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_window.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

LockWindow::LockWindow() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // Shell may be null in tests.
  if (Shell::HasInstance()) {
    params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                        kShellWindowId_LockScreenContainer);
  }
  Init(params);
  GetGestureRecognizer()->CancelActiveTouchesExcept(nullptr);
  SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  // Disable virtual keyboard overscroll because it interferes with scrolling
  // login/lock content. See crbug.com/363635.
  keyboard::mojom::KeyboardConfig config =
      keyboard::KeyboardController::Get()->keyboard_config();
  config.overscroll_behavior =
      keyboard::mojom::KeyboardOverscrollBehavior::kDisabled;
  keyboard::KeyboardController::Get()->UpdateKeyboardConfig(config);
}

LockWindow::~LockWindow() {
  keyboard::mojom::KeyboardConfig config =
      keyboard::KeyboardController::Get()->keyboard_config();
  config.overscroll_behavior =
      keyboard::mojom::KeyboardOverscrollBehavior::kDefault;
  keyboard::KeyboardController::Get()->UpdateKeyboardConfig(config);

  // We need to destroy the root view before destroying |data_dispatcher_|
  // because lock screen destruction assumes it is alive. We could hand out
  // base::WeakPtr<LoginDataDispatcher> instances if needed instead.
  delete views::Widget::GetContentsView();
}

views::Widget* LockWindow::GetWidget() {
  return this;
}

const views::Widget* LockWindow::GetWidget() const {
  return this;
}

views::View* LockWindow::GetInitiallyFocusedView() {
  // There are multiple GetContentsView definitions; use the views::Widget one.
  return views::Widget::GetContentsView();
}

}  // namespace ash
