// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_
#define ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/touch_selection/touch_selection_magnifier_runner.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class SelectionBound;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Ash implementation for TouchSelectionMagnifierRunner.
class ASH_EXPORT TouchSelectionMagnifierRunnerAsh
    : public ui::TouchSelectionMagnifierRunner,
      public ui::ColorProviderSourceObserver {
 public:
  TouchSelectionMagnifierRunnerAsh();

  TouchSelectionMagnifierRunnerAsh(const TouchSelectionMagnifierRunnerAsh&) =
      delete;
  TouchSelectionMagnifierRunnerAsh& operator=(
      const TouchSelectionMagnifierRunnerAsh&) = delete;

  ~TouchSelectionMagnifierRunnerAsh() override;

  // ui::TouchSelectionMagnifierRunner:
  void ShowMagnifier(aura::Window* context,
                     const gfx::SelectionBound& focus_bound) override;
  void CloseMagnifier() override;
  bool IsRunning() const override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  const aura::Window* GetCurrentContextForTesting() const;
  const ui::Layer* GetMagnifierLayerForTesting() const;

 private:
  class BorderRenderer;

  void CreateMagnifierLayer();

  // Current context window in which the magnifier is being shown, or `nullptr`
  // if no magnifier is running.
  raw_ptr<aura::Window> current_context_ = nullptr;

  // The magnifier layer is the parent of the zoom layer and border layer. The
  // layer bounds should be updated when selection updates occur.
  std::unique_ptr<ui::Layer> magnifier_layer_;

  // Draws the magnified area, i.e. the background with a zoom and offset filter
  // applied.
  std::unique_ptr<ui::Layer> zoom_layer_;

  // Draws the magnifier border and shadows. `border_layer_` must be ordered
  // after `border_renderer_` so that it is destroyed before `border_renderer_`.
  // Otherwise `border_layer_` will have a pointer to a deleted delegate.
  std::unique_ptr<BorderRenderer> border_renderer_;
  std::unique_ptr<ui::Layer> border_layer_;
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_SELECTION_MAGNIFIER_RUNNER_ASH_H_
