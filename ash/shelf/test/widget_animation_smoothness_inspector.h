// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_TEST_WIDGET_ANIMATION_SMOOTHNESS_INSPECTOR_H_
#define ASH_SHELF_TEST_WIDGET_ANIMATION_SMOOTHNESS_INSPECTOR_H_

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_observer.h"

namespace views {
class Widget;
}

namespace ash {

// TOOD(manucornet): This class will perform its intended purpose only after
// the new API for layer aimation sequence observers lands.
class WidgetAnimationSmoothnessInspector : ui::LayerAnimationObserver {
 public:
  explicit WidgetAnimationSmoothnessInspector(views::Widget* widget);
  ~WidgetAnimationSmoothnessInspector() override;

  // Returns whether the animation had at least |min_steps| steps (including
  // the initial and final steps), going smoothly from the initial state
  // to the final state.
  bool CheckAnimation(unsigned int min_steps) const;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // TOOD(manucornet): Make this an override once the new API for layer
  // aimation sequence observers is in.
  void OnLayerAnimationProgressed(const ui::LayerAnimationSequence* sequence);

 private:
  // Unowned
  raw_ptr<views::Widget> widget_;

  std::vector<gfx::Rect> bound_history_;
};

}  // namespace ash

#endif  // ASH_SHELF_TEST_WIDGET_ANIMATION_SMOOTHNESS_INSPECTOR_H_
