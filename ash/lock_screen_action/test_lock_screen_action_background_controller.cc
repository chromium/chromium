// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/test_lock_screen_action_background_controller.h"

#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const char kTestingWidgetName[] = "TestingLockScreenActionBackgroundWidget";

}  // namespace

TestLockScreenActionBackgroundController::
    TestLockScreenActionBackgroundController() = default;

TestLockScreenActionBackgroundController::
    ~TestLockScreenActionBackgroundController() = default;

bool TestLockScreenActionBackgroundController::IsBackgroundWindow(
    aura::Window* window) const {
  // Cannot check compare |window| to |widget_| window because this might get
  // called before |widget_|'s native window is set (while the window is being
  // added to the layout manager). Test the window name instead.
  return window->GetName() == kTestingWidgetName;
}

bool TestLockScreenActionBackgroundController::ShowBackground() {
  if (state() == LockScreenActionBackgroundState::kShowing ||
      state() == LockScreenActionBackgroundState::kShown) {
    return false;
  }

  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.name = kTestingWidgetName;
    params.parent = parent_window_;
    params.delegate = new views::WidgetDelegate();
    params.delegate->SetCanActivate(false);
    params.delegate->SetCanMaximize(true);
    params.delegate->SetCanFullscreen(true);
    params.delegate->SetCanResize(true);
    params.delegate->SetOwnedByWidget(true);
    params.delegate->SetFocusTraversesOut(true);

    widget_->Init(std::move(params));
  }

  widget_->Show();

  UpdateState(LockScreenActionBackgroundState::kShowing);
  return true;
}

bool TestLockScreenActionBackgroundController::HideBackgroundImmediately() {
  if (state() == LockScreenActionBackgroundState::kHidden)
    return false;

  widget_->Hide();
  UpdateState(LockScreenActionBackgroundState::kHidden);
  return true;
}

bool TestLockScreenActionBackgroundController::HideBackground() {
  if (state() == LockScreenActionBackgroundState::kHiding ||
      state() == LockScreenActionBackgroundState::kHidden) {
    return false;
  }

  UpdateState(LockScreenActionBackgroundState::kHiding);
  return true;
}

bool TestLockScreenActionBackgroundController::FinishShow() {
  if (state() != LockScreenActionBackgroundState::kShowing)
    return false;
  UpdateState(LockScreenActionBackgroundState::kShown);
  return true;
}

bool TestLockScreenActionBackgroundController::FinishHide() {
  if (state() != LockScreenActionBackgroundState::kHiding)
    return false;
  widget_->Hide();
  UpdateState(LockScreenActionBackgroundState::kHidden);
  return true;
}

aura::Window* TestLockScreenActionBackgroundController::GetWindow() const {
  if (!widget_)
    return nullptr;
  return widget_->GetNativeWindow();
}

}  // namespace ash
