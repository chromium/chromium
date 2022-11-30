// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/widget_animation_smoothness_inspector.h"

#include "base/strings/stringprintf.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/widget/widget.h"

namespace ash {

const char kErrorFormat[] =
    "Animation changes the widget's %s but stutters at value %d between steps "
    "%lu and %lu (on a total of %lu steps)";

WidgetAnimationSmoothnessInspector::WidgetAnimationSmoothnessInspector(
    views::Widget* widget)
    : widget_(widget) {
  widget->GetLayer()->GetAnimator()->AddObserver(this);
}

WidgetAnimationSmoothnessInspector::~WidgetAnimationSmoothnessInspector() {
  widget_->GetLayer()->GetAnimator()->RemoveObserver(this);
}

bool WidgetAnimationSmoothnessInspector::CheckAnimation(
    unsigned int min_steps) const {
  DCHECK(min_steps > 2) << "An animation with 2 steps or less isn't "
                        << "really an animation";

  if (bound_history_.size() < min_steps)
    return false;
  const unsigned long total_step_count = bound_history_.size();
  const unsigned long interval_between_steps_to_check =
      total_step_count / (min_steps - 1);
  const gfx::Rect initial_bounds = bound_history_.front();
  const gfx::Rect final_bounds = bound_history_.back();
  const bool x_changed = initial_bounds.x() != final_bounds.x();
  const bool y_changed = initial_bounds.y() != final_bounds.y();
  const bool w_changed = initial_bounds.width() != final_bounds.width();
  const bool h_changed = initial_bounds.height() != final_bounds.height();

  // An animation that changes nothing can't be considered smooth.
  if (!x_changed && !y_changed && !w_changed && !h_changed)
    return false;

  int last_x = initial_bounds.x();
  int last_y = initial_bounds.y();
  int last_w = initial_bounds.width();
  int last_h = initial_bounds.height();

  auto print_error = [=](const char* dimension, int value,
                         unsigned long step) -> void {
    // Step numbers are 1-based in user-visible messages.
    DLOG(ERROR) << base::StringPrintf(
        kErrorFormat, dimension, value,
        1 + step - interval_between_steps_to_check, 1 + step, total_step_count);
  };

  for (unsigned long i = interval_between_steps_to_check; i < total_step_count;
       i += interval_between_steps_to_check) {
    // Check the actual last step on the last loop iteration.
    if ((total_step_count - i) < interval_between_steps_to_check)
      i = total_step_count - 1;

    const gfx::Rect bounds = bound_history_[i];
    if (x_changed && bounds.x() == last_x) {
      print_error("x", last_x, i);
      return false;
    }
    if (y_changed && bounds.y() == last_y) {
      print_error("y", last_y, i);
      return false;
    }
    if (w_changed && bounds.width() == last_y) {
      print_error("width", last_w, i);
      return false;
    }
    if (h_changed && bounds.height() == last_h) {
      print_error("height", last_h, i);
      return false;
    }
    last_x = bounds.x();
    last_y = bounds.y();
    last_w = bounds.width();
    last_h = bounds.height();
  }
  return true;
}

void WidgetAnimationSmoothnessInspector::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  bound_history_.push_back(widget_->GetClientAreaBoundsInScreen());
}

void WidgetAnimationSmoothnessInspector::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {}

void WidgetAnimationSmoothnessInspector::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  bound_history_.push_back(widget_->GetClientAreaBoundsInScreen());
}

void WidgetAnimationSmoothnessInspector::OnLayerAnimationProgressed(
    const ui::LayerAnimationSequence* sequence) {
  bound_history_.push_back(widget_->GetClientAreaBoundsInScreen());
}

}  // namespace ash
