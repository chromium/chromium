// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_controller_impl.h"

#include "ash/lock_screen_action/lock_screen_action_background_view.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

constexpr char kLockScreenActionBackgroundWidgetName[] =
    "LockScreenActionBackground";

}  // namespace

LockScreenActionBackgroundControllerImpl::
    LockScreenActionBackgroundControllerImpl() = default;

LockScreenActionBackgroundControllerImpl::
    ~LockScreenActionBackgroundControllerImpl() {
  if (background_widget_ && !background_widget_->IsClosed())
    background_widget_->Close();
}

bool LockScreenActionBackgroundControllerImpl::IsBackgroundWindow(
    aura::Window* window) const {
  return window->GetName() == kLockScreenActionBackgroundWidgetName;
}

bool LockScreenActionBackgroundControllerImpl::ShowBackground() {
  if (state() == LockScreenActionBackgroundState::kShown ||
      state() == LockScreenActionBackgroundState::kShowing) {
    return false;
  }

  if (!parent_window_)
    return false;

  if (!background_widget_)
    background_widget_ = CreateWidget();

  UpdateState(LockScreenActionBackgroundState::kShowing);

  background_widget_->Show();

  contents_view_->AnimateShow(base::BindOnce(
      &LockScreenActionBackgroundControllerImpl::OnBackgroundShown,
      weak_ptr_factory_.GetWeakPtr()));

  return true;
}

bool LockScreenActionBackgroundControllerImpl::HideBackgroundImmediately() {
  if (state() == LockScreenActionBackgroundState::kHidden)
    return false;

  UpdateState(LockScreenActionBackgroundState::kHidden);

  background_widget_->Hide();
  return true;
}

bool LockScreenActionBackgroundControllerImpl::HideBackground() {
  if (state() == LockScreenActionBackgroundState::kHidden ||
      state() == LockScreenActionBackgroundState::kHiding) {
    return false;
  }

  DCHECK(background_widget_);

  UpdateState(LockScreenActionBackgroundState::kHiding);

  contents_view_->AnimateHide(base::BindOnce(
      &LockScreenActionBackgroundControllerImpl::OnBackgroundHidden,
      weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void LockScreenActionBackgroundControllerImpl::OnWidgetDestroyed(
    views::Widget* widget) {
  if (widget != background_widget_)
    return;
  DCHECK(widget_observation_.IsObservingSource(widget));
  widget_observation_.Reset();

  background_widget_ = nullptr;
  contents_view_ = nullptr;

  UpdateState(LockScreenActionBackgroundState::kHidden);
}

views::Widget* LockScreenActionBackgroundControllerImpl::CreateWidget() {
  // Passed to the widget as its delegate.
  contents_view_ = new LockScreenActionBackgroundView();

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = kLockScreenActionBackgroundWidgetName;
  params.parent = parent_window_;
  params.delegate = contents_view_.get();

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget_observation_.Observe(widget);

  return widget;
}

void LockScreenActionBackgroundControllerImpl::OnBackgroundShown() {
  if (state() != LockScreenActionBackgroundState::kShowing)
    return;

  UpdateState(LockScreenActionBackgroundState::kShown);
}

void LockScreenActionBackgroundControllerImpl::OnBackgroundHidden() {
  if (state() != LockScreenActionBackgroundState::kHiding)
    return;

  background_widget_->Hide();

  UpdateState(LockScreenActionBackgroundState::kHidden);
}

}  // namespace ash
