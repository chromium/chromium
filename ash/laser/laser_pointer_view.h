// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include "ash/components/fast_ink/fast_ink_points.h"
#include "ash/components/fast_ink/fast_ink_view.h"
#include "base/timer/timer.h"

namespace ash {

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public fast_ink::FastInkView {
 public:
  LaserPointerView(base::TimeDelta life_duration,
                   base::TimeDelta presentation_delay,
                   base::TimeDelta stationary_point_delay,
                   aura::Window* container);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::PointF& new_point,
                   const base::TimeTicks& new_time);
  void FadeOut(base::OnceClosure done);

 private:
  friend class LaserPointerControllerTestApi;

  void AddPoint(const gfx::PointF& point, const base::TimeTicks& time);
  void ScheduleUpdateBuffer();
  void UpdateBuffer();
  // Timer callback which adds a point where the stylus was last seen.
  // This allows the trail to fade away when the stylus is stationary.
  void UpdateTime();
  gfx::Rect GetBoundingBox();
  void Draw(gfx::Canvas& canvas);

  fast_ink::FastInkPoints laser_points_;
  fast_ink::FastInkPoints predicted_laser_points_;
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

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
