// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_
#define ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_

#include <memory>
#include <optional>

#include "ash/fast_ink/fast_ink_view.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// CursorView class can be used to display cursor images with minimal
// latency/jank.
class CursorView : public FastInkView {
 public:
  CursorView(const CursorView&) = delete;
  CursorView& operator=(const CursorView&) = delete;
  ~CursorView() override;

  static views::UniqueWidgetPtr Create(const gfx::Point& initial_location,
                                       aura::Window* container);

  void SetCursorImages(const std::vector<gfx::ImageSkia>& cursor_image,
                       const gfx::Size& cursor_size,
                       const gfx::Point& cursor_hotspot);
  void SetLocation(const gfx::Point& location);

  const gfx::Rect& get_cursor_rect_for_test() const { return cursor_rect_; }

 private:
  CursorView(const gfx::Point& initial_location);

  // Initialize CursorView after FaskInkHost is setup.
  void Init();

  // Update cursor animation.
  void UpdateAnimation();

  // Advance `cursor_image_index_` to the next frame.
  void AdvanceFrame();

  // Draw the current cursor.
  void Draw();

  // Stop the `stationary_timer_`.
  void OnStationary();

  // Update `cursor_rect_` and `damage_rect_` and draw cursor.
  void UpdateCursor();

  gfx::Transform buffer_to_screen_transform_;
  float device_scale_factor_ = 1.0f;

  gfx::Point cursor_location_;
  std::vector<gfx::ImageSkia> cursor_images_;
  int cursor_image_index_ = 0;

  // Cursor hotspot relative to the origin of cursor image in dp.
  // It is the single point within the cursor image that is considered to be the
  // cursor's point of interaction with other elements on the screen
  gfx::Point cursor_hotspot_;

  // Cursor size in dp.
  gfx::Size cursor_size_;

  // Cursor image bounds in root window coordinates.
  gfx::Rect cursor_rect_;

  // Damage rect in root window coordinates.
  gfx::Rect damage_rect_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Timer for cursor's stationary status. The cursor gets into stationary state
  // after it is not moved for a certain period of time, which is tracked by
  // this timer.
  std::optional<base::RetainingOneShotTimer> stationary_timer_;

  // Timer for animated cursor drawing.
  base::RepeatingTimer animated_cursor_timer_;

  base::WeakPtrFactory<CursorView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_
