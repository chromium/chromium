// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_TEST_API_H_
#define ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_TEST_API_H_

#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class Layer;
}

namespace ash {

// Use the api in this class to test the internals of
// RootWindowDeskSwitchAnimator.
class RootWindowDeskSwitchAnimatorTestApi {
 public:
  explicit RootWindowDeskSwitchAnimatorTestApi(
      RootWindowDeskSwitchAnimator* animator);
  RootWindowDeskSwitchAnimatorTestApi(
      const RootWindowDeskSwitchAnimatorTestApi&) = delete;
  RootWindowDeskSwitchAnimatorTestApi& operator=(
      const RootWindowDeskSwitchAnimatorTestApi&) = delete;
  ~RootWindowDeskSwitchAnimatorTestApi();

  // Getters for the layers associated with the animation.
  ui::Layer* GetAnimationLayer();
  ui::Layer* GetScreenshotLayerOfDeskWithIndex(int desk_index);

  DeskSwitchAnimationType GetAnimatorType() const;

  int GetEndingDeskIndex() const;

  void SetOnStartingScreenshotTakenCallback(base::OnceClosure callback);
  void SetOnEndingScreenshotTakenCallback(base::OnceClosure callback);

 private:
  const raw_ptr<RootWindowDeskSwitchAnimator, DanglingUntriaged> animator_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_TEST_API_H_
