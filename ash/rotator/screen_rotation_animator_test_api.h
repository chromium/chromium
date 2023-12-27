// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_TEST_API_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_TEST_API_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class Layer;
}

namespace ash {
class ScreenRotationAnimator;

// Test API to provide internal access to a ScreenRotationAnimator instance.
// This can also be used to control the screen rotation animations via the
// ui::test::MultiLayerAnimatorTestController API.
class ScreenRotationAnimatorTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit ScreenRotationAnimatorTestApi(ScreenRotationAnimator* animator);

  ScreenRotationAnimatorTestApi(const ScreenRotationAnimatorTestApi&) = delete;
  ScreenRotationAnimatorTestApi& operator=(
      const ScreenRotationAnimatorTestApi&) = delete;

  ~ScreenRotationAnimatorTestApi() override;

  // Wrapper functions for ScreenRotationAnimator.
  ui::Layer* GetOldLayerTreeRootLayer();
  void DisableAnimationTimers();

 private:
  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

  raw_ptr<ScreenRotationAnimator, DanglingUntriaged> animator_;
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_TEST_API_H_
