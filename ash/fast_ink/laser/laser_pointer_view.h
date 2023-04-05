// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_LASER_LASER_POINTER_VIEW_H_
#define ASH_FAST_INK_LASER_LASER_POINTER_VIEW_H_

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/fast_ink/fast_ink_view.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public FastInkView {
 public:
  LaserPointerView(const LaserPointerView&) = delete;
  LaserPointerView& operator=(const LaserPointerView&) = delete;

  ~LaserPointerView() override;

  // Function to create a container Widget, initialize |cursor_view| and
  // pass ownership as the contents view to the Widget.
  static views::UniqueWidgetPtr Create(base::TimeDelta life_duration,
                                       base::TimeDelta presentation_delay,
                                       base::TimeDelta stationary_point_delay,
                                       aura::Window* container);

  void AddNewPoint(const gfx::PointF& new_point,
                   const base::TimeTicks& new_time);
  void FadeOut(base::OnceClosure done);

 private:
  friend class LaserPointerControllerTestApi;

  LaserPointerView(base::TimeDelta life_duration,
                   base::TimeDelta presentation_delay,
                   base::TimeDelta stationary_point_delay);

  void AddPoint(const gfx::PointF& point, const base::TimeTicks& time);
  void ScheduleUpdateBuffer();
  void UpdateBuffer();
  // Timer callback which adds a point where the stylus was last seen.
  // This allows the trail to fade away when the stylus is stationary.
  void UpdateTime();
  gfx::Rect GetBoundingBox();
  void Draw(gfx::Canvas& canvas);

  FastInkPoints laser_points_;
  FastInkPoints predicted_laser_points_;
  const base::TimeDelta presentation_delay_;
  // Timer which will add a new stationary point when the stylus stops moving.
  // This will remove points that are too old.
  base::RepeatingTimer stationary_timer_;
  gfx::PointF stationary_point_location_;
  // A callback for when the fadeout is complete.
  base::OnceClosure fadeout_done_;
  gfx::Rect laser_content_rect_;
  bool pending_update_buffer_ = false;
  base::WeakPtrFactory<LaserPointerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_LASER_LASER_POINTER_VIEW_H_
