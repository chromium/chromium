// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_
#define ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_

#include <memory>

#include "ash/fast_ink/fast_ink_view.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
struct PresentationFeedback;
}

namespace ash {

// CursorView class can be used to display a cursor image with minimal
// latency/jank and optional motion blur.
class CursorView : public FastInkView,
                   public ui::CursorController::CursorObserver {
 public:
  CursorView(const CursorView&) = delete;
  CursorView& operator=(const CursorView&) = delete;

  ~CursorView() override;

  static views::UniqueWidgetPtr Create(const gfx::Point& initial_location,
                                       bool is_motion_blur_enabled,
                                       aura::Window* container);

  void SetCursorImage(const gfx::ImageSkia& cursor_image,
                      const gfx::Size& cursor_size,
                      const gfx::Point& cursor_hotspot);

  // ui::CursorController::CursorObserver overrides:
  void OnCursorLocationChanged(const gfx::PointF& location) override;

 protected:
  // ash::FastInkView overrides:
  FastInkHost::PresentationCallback GetPresentationCallback() override;

 private:
  // Paints cursor on the paint thread.
  class Painter;

  CursorView(const gfx::Point& initial_location, bool is_motion_blur_enabled);

  // Initialize CursorView after FaskInkHost is setup.
  void Init();

  // Invoked when a frame is presented.
  void DidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

  // Constants that can be used on any thread.
  gfx::Transform buffer_to_screen_transform_;

  std::unique_ptr<Painter> painter_;

  // UI thread state.
  raw_ptr<ui::Compositor, ExperimentalAsh> compositor_ = nullptr;
  SEQUENCE_CHECKER(ui_sequence_checker_);
  base::WeakPtrFactory<CursorView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_CURSOR_CURSOR_VIEW_H_
