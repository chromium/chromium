// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include <memory>
#include <utility>

#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/highlighter/highlighter_result_view.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/system/palette/palette_utils.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Bezel stroke detection margin, in DP.
const int kScreenEdgeMargin = 2;

const int kInterruptedStrokeTimeoutMs = 500;

// Adjust the height of the bounding box to match the pen tip height,
// while keeping the same vertical center line. Adjust the width to
// account for the pen tip width.
gfx::RectF AdjustHorizontalStroke(const gfx::RectF& box,
                                  const gfx::SizeF& pen_tip_size) {
  return gfx::RectF(box.x() - pen_tip_size.width() / 2,
                    box.CenterPoint().y() - pen_tip_size.height() / 2,
                    box.width() + pen_tip_size.width(), pen_tip_size.height());
}

}  // namespace

HighlighterController::HighlighterController() {
  Shell::Get()->AddPreTargetHandler(this);
}

HighlighterController::~HighlighterController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void HighlighterController::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void HighlighterController::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void HighlighterController::SetExitCallback(base::OnceClosure exit_callback,
                                            bool require_success) {
  exit_callback_ = std::move(exit_callback);
  require_success_ = require_success;
}

void HighlighterController::UpdateEnabledState(
    HighlighterEnabledState enabled_state) {
  if (enabled_state_ == enabled_state)
    return;
  enabled_state_ = enabled_state;

  SetEnabled(enabled_state == HighlighterEnabledState::kEnabled);
  for (auto& observer : observers_)
    observer.OnHighlighterEnabledChanged(enabled_state);
}

void HighlighterController::AbortSession() {
  if (enabled_state_ == HighlighterEnabledState::kEnabled)
    UpdateEnabledState(HighlighterEnabledState::kDisabledBySessionAbort);
}

void HighlighterController::SetEnabled(bool enabled) {
  FastInkPointerController::SetEnabled(enabled);
  if (enabled) {
    session_start_ = ui::EventTimeForNow();
    gesture_counter_ = 0;
    recognized_gesture_counter_ = 0;
  } else {
    UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Palette.Assistant.GesturesPerSession",
                             gesture_counter_);
    UMA_HISTOGRAM_COUNTS_100(
        "Ash.Shelf.Palette.Assistant.GesturesPerSession.Recognized",
        recognized_gesture_counter_);

    // If |highlighter_view_| is animating it will delete itself when done
    // animating. |result_view_| will exist only if |highlighter_view_| is
    // animating, and it will also delete itself when done animating.
    if (highlighter_view_ && !highlighter_view_->animating())
      DestroyPointerView();
  }
}

views::View* HighlighterController::GetPointerView() const {
  return highlighter_view_.get();
}

void HighlighterController::CreatePointerView(
    base::TimeDelta presentation_delay,
    aura::Window* root_window) {
  highlighter_view_ = std::make_unique<HighlighterView>(
      presentation_delay,
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  result_view_.reset();
}

void HighlighterController::UpdatePointerView(ui::TouchEvent* event) {
  interrupted_stroke_timer_.reset();

  highlighter_view_->AddNewPoint(event->root_location_f(), event->time_stamp());

  if (event->type() != ui::ET_TOUCH_RELEASED)
    return;

  gfx::Rect bounds = highlighter_view_->GetWidget()
                         ->GetNativeWindow()
                         ->GetRootWindow()
                         ->bounds();
  bounds.Inset(kScreenEdgeMargin, kScreenEdgeMargin);

  const gfx::PointF pos = highlighter_view_->points().GetNewest().location;
  if (bounds.Contains(
          gfx::Point(static_cast<int>(pos.x()), static_cast<int>(pos.y())))) {
    // The stroke has ended far enough from the screen edge, process it
    // immediately.
    RecognizeGesture();
    return;
  }

  // The stroke has ended close to the screen edge. Delay gesture recognition
  // a little to give the pen a chance to re-enter the screen.
  highlighter_view_->AddGap();

  interrupted_stroke_timer_ = std::make_unique<base::OneShotTimer>();
  interrupted_stroke_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kInterruptedStrokeTimeoutMs),
      base::Bind(&HighlighterController::RecognizeGesture,
                 base::Unretained(this)));
}

void HighlighterController::RecognizeGesture() {
  interrupted_stroke_timer_.reset();

  aura::Window* current_window =
      highlighter_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  const gfx::Rect bounds = current_window->bounds();

  const fast_ink::FastInkPoints& points = highlighter_view_->points();
  gfx::RectF box = points.GetBoundingBoxF();

  const HighlighterGestureType gesture_type =
      DetectHighlighterGesture(box, HighlighterView::kPenTipSize, points);

  if (gesture_type == HighlighterGestureType::kHorizontalStroke) {
    UMA_HISTOGRAM_COUNTS_10000("Ash.Shelf.Palette.Assistant.HighlighterLength",
                               static_cast<int>(box.width()));

    box = AdjustHorizontalStroke(box, HighlighterView::kPenTipSize);
  } else if (gesture_type == HighlighterGestureType::kClosedShape) {
    const float fraction =
        box.width() * box.height() / (bounds.width() * bounds.height());
    UMA_HISTOGRAM_PERCENTAGE("Ash.Shelf.Palette.Assistant.CircledPercentage",
                             static_cast<int>(fraction * 100));
  }

  highlighter_view_->Animate(
      box.CenterPoint(), gesture_type,
      base::Bind(&HighlighterController::DestroyHighlighterView,
                 base::Unretained(this)));

  // |box| is not guaranteed to be inside the screen bounds, clip it.
  // Not converting |box| to gfx::Rect here to avoid accumulating rounding
  // errors, instead converting |bounds| to gfx::RectF.
  box.Intersect(
      gfx::RectF(bounds.x(), bounds.y(), bounds.width(), bounds.height()));

  if (!box.IsEmpty() &&
      gesture_type != HighlighterGestureType::kNotRecognized) {
    // The window for selection should be the root window to show assistant.
    Shell::Get()->shell_state()->SetRootWindowForNewWindows(
        current_window->GetRootWindow());

    const gfx::Rect selection_rect = gfx::ToEnclosingRect(box);
    for (auto& observer : observers_)
      observer.OnHighlighterSelectionRecognized(selection_rect);

    result_view_ = std::make_unique<HighlighterResultView>(current_window);
    result_view_->Animate(box, gesture_type,
                          base::Bind(&HighlighterController::DestroyResultView,
                                     base::Unretained(this)));

    recognized_gesture_counter_++;
    CallExitCallback();
  } else {
    if (!require_success_)
      CallExitCallback();
  }

  gesture_counter_++;

  const base::TimeTicks gesture_start = points.GetOldest().time;
  if (gesture_counter_ > 1) {
    // Up to 3 minutes.
    UMA_HISTOGRAM_MEDIUM_TIMES("Ash.Shelf.Palette.Assistant.GestureInterval",
                               gesture_start - previous_gesture_end_);
  }
  previous_gesture_end_ = points.GetNewest().time;

  // Up to 10 seconds.
  UMA_HISTOGRAM_TIMES("Ash.Shelf.Palette.Assistant.GestureDuration",
                      points.GetNewest().time - gesture_start);

  UMA_HISTOGRAM_ENUMERATION("Ash.Shelf.Palette.Assistant.GestureType",
                            gesture_type,
                            HighlighterGestureType::kGestureCount);
}

void HighlighterController::DestroyPointerView() {
  DestroyHighlighterView();
  DestroyResultView();
}

bool HighlighterController::CanStartNewGesture(ui::TouchEvent* event) {
  // Ignore events over the palette.
  if (ash::palette_utils::PaletteContainsPointInScreen(event->root_location()))
    return false;
  return !interrupted_stroke_timer_ &&
         FastInkPointerController::CanStartNewGesture(event);
}

void HighlighterController::DestroyHighlighterView() {
  highlighter_view_.reset();
  // |interrupted_stroke_timer_| should never be non null when
  // |highlighter_view_| is null.
  interrupted_stroke_timer_.reset();
}

void HighlighterController::DestroyResultView() {
  result_view_.reset();
}

void HighlighterController::CallExitCallback() {
  if (!exit_callback_.is_null())
    std::move(exit_callback_).Run();
}

}  // namespace ash
