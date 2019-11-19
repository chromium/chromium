// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/test/keyboard_test_util.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "ui/display/screen.h"

namespace keyboard {

namespace {

class KeyboardVisibilityChangeWaiter : public ash::KeyboardControllerObserver {
 public:
  explicit KeyboardVisibilityChangeWaiter(bool wait_until)
      : wait_until_(wait_until) {
    KeyboardUIController::Get()->AddObserver(this);
  }
  ~KeyboardVisibilityChangeWaiter() override {
    KeyboardUIController::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  void OnKeyboardVisibilityChanged(const bool is_visible) override {
    if (is_visible == wait_until_)
      run_loop_.QuitWhenIdle();
  }

  base::RunLoop run_loop_;
  const bool wait_until_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardVisibilityChangeWaiter);
};

bool WaitVisibilityChangesTo(bool wait_until) {
  if (KeyboardUIController::Get()->IsKeyboardVisible() == wait_until)
    return true;
  KeyboardVisibilityChangeWaiter waiter(wait_until);
  waiter.Wait();
  return true;
}

}  // namespace

namespace test {

bool WaitUntilLoaded() {
  // In tests, the keyboard window is mocked out so it usually "loads" within a
  // single RunUntilIdle call.
  base::RunLoop run_loop;
  while (KeyboardUIController::Get()->GetStateForTest() ==
         KeyboardUIState::kLoading) {
    run_loop.RunUntilIdle();
  }
  return true;
}

}  // namespace test

bool WaitUntilShown() {
  // KeyboardController send a visibility update once the show animation
  // finishes.
  return WaitVisibilityChangesTo(true /* wait_until */);
}

bool WaitUntilHidden() {
  // Unlike |WaitUntilShown|, KeyboardController updates its visibility
  // at the beginning of the hide animation. There's currently no way to
  // actually detect when the hide animation finishes.
  // TODO(https://crbug.com/849995): Find a proper solution to this.
  return WaitVisibilityChangesTo(false /* wait_until */);
}

bool IsKeyboardShowing() {
  auto* keyboard_controller = KeyboardUIController::Get();
  DCHECK(keyboard_controller->IsEnabled());

  // KeyboardController sets its state to SHOWN when it is about to show.
  return keyboard_controller->GetStateForTest() == KeyboardUIState::kShown;
}

bool IsKeyboardHiding() {
  auto* keyboard_controller = KeyboardUIController::Get();
  DCHECK(keyboard_controller->IsEnabled());

  return keyboard_controller->GetStateForTest() == KeyboardUIState::kWillHide ||
         keyboard_controller->GetStateForTest() == KeyboardUIState::kHidden;
}

gfx::Rect KeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                       int keyboard_height) {
  return gfx::Rect(root_bounds.x(), root_bounds.bottom() - keyboard_height,
                   root_bounds.width(), keyboard_height);
}

}  // namespace keyboard
