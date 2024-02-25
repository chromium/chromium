// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_test_helper.h"
#include "base/memory/raw_ptr.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/run_loop.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

// An observer that quits a run loop when the animation finishes.
class AnimationEndObserver : public ui::LayerAnimationObserver {
 public:
  explicit AnimationEndObserver(ui::LayerAnimator* animator)
      : animator_(animator) {
    animator_->AddObserver(this);
  }
  ~AnimationEndObserver() override { animator_->RemoveObserver(this); }

  void WaitForAnimationEnd() {
    if (!animator_->is_animating())
      return;
    // This will return immediately if |Quit| was already called.
    run_loop_.Run();
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    run_loop_.Quit();
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  raw_ptr<ui::LayerAnimator> animator_;
  base::RunLoop run_loop_;
};

LoginStatus StatusAreaWidgetTestHelper::GetUserLoginStatus() {
  return Shell::Get()->session_controller()->login_status();
}

StatusAreaWidget* StatusAreaWidgetTestHelper::GetStatusAreaWidget() {
  return Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();
}

StatusAreaWidget* StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget() {
  RootWindowController* primary_controller =
      Shell::GetPrimaryRootWindowController();
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    if (controllers[i] != primary_controller)
      return controllers[i]->GetStatusAreaWidget();
  }

  return nullptr;
}

void StatusAreaWidgetTestHelper::WaitForAnimationEnd(
    StatusAreaWidget* status_area_widget) {
  AnimationEndObserver observer(status_area_widget->GetLayer()->GetAnimator());
  observer.WaitForAnimationEnd();
  status_area_widget->GetLayer()->GetAnimator()->StopAnimating();
}

void StatusAreaWidgetTestHelper::WaitForLayerAnimationEnd(ui::Layer* layer) {
  AnimationEndObserver observer(layer->GetAnimator());
  observer.WaitForAnimationEnd();
}

}  // namespace ash
