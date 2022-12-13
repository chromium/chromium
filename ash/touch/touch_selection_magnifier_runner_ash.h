// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_
#define ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/touch_selection/touch_selection_magnifier_runner.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Ash implementation for TouchSelectionMagnifierRunner.
class ASH_EXPORT TouchSelectionMagnifierRunnerAsh
    : public ui::TouchSelectionMagnifierRunner {
 public:
  TouchSelectionMagnifierRunnerAsh();

  TouchSelectionMagnifierRunnerAsh(const TouchSelectionMagnifierRunnerAsh&) =
      delete;
  TouchSelectionMagnifierRunnerAsh& operator=(
      const TouchSelectionMagnifierRunnerAsh&) = delete;

  ~TouchSelectionMagnifierRunnerAsh() override;

  static constexpr float kMagnifierScale = 2.0f;

  static constexpr gfx::Size kMagnifierLayerSize{100, 48};

  static constexpr gfx::RoundedCornersF kMagnifierRoundedCorners{20};

  // Offset to apply so that the magnifier is shown vertically above the point
  // of interest. The offset specifies vertical displacement from the center of
  // the text selection caret to the center of the magnifier bounds.
  static constexpr int kMagnifierVerticalOffset = -32;

  // ui::TouchSelectionMagnifierRunner:
  void ShowMagnifier(aura::Window* context,
                     const gfx::PointF& position) override;
  void CloseMagnifier() override;
  bool IsRunning() const override;

  const aura::Window* GetCurrentContextForTesting() const;

  const ui::Layer* GetMagnifierLayerForTesting() const;

 private:
  void CreateMagnifierLayer(aura::Window* root_window,
                            const gfx::PointF& position_in_root);

  // Current context window in which the magnifier is being shown, or `nullptr`
  // if no magnifier is running.
  raw_ptr<aura::Window> current_context_ = nullptr;

  // The magnifier layer, which draws the background with a zoom filter applied.
  std::unique_ptr<ui::Layer> magnifier_layer_;
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_
