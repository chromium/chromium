// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_alignment_controller.h"

#include "ash/display/display_alignment_indicator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/timer/timer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// Number of times the mouse has to hit the edge to show the indicators.
constexpr int kTriggerThresholdCount = 2;
// Time between last time the mouse leaves a screen edge and the counter
// resetting.
constexpr base::TimeDelta kCounterResetTime = base::Seconds(1);
// How long the indicators are visible for.
constexpr base::TimeDelta kIndicatorVisibilityDuration = base::Seconds(2);

// Returns true if |screen_location| is on the edge of |display|. |display| must
// be valid.
bool IsOnBoundary(const gfx::Point& screen_location,
                  const display::Display& display) {
  DCHECK(display.is_valid());

  const gfx::Rect& bounds = display.bounds();

  const int top = bounds.y();
  const int bottom = bounds.bottom() - 1;
  const int left = bounds.x();
  const int right = bounds.right() - 1;

  // See if current screen_location is within 1px of the display's
  // borders. 1px leniency is necessary as some resolution/size factor
  // combination results in the mouse not being able to reach the edges of the
  // display by 1px.
  if (std::abs(screen_location.x() - left) <= 1)
    return true;
  if (std::abs(screen_location.x() - right) <= 1)
    return true;
  if (std::abs(screen_location.y() - top) <= 1)
    return true;

  return std::abs(screen_location.y() - bottom) <= 1;
}

}  // namespace

DisplayAlignmentController::DisplayAlignmentController()
    : action_trigger_timer_(std::make_unique<base::OneShotTimer>()) {
  Shell* shell = Shell::Get();
  shell->AddPreTargetHandler(this);
  shell->session_controller()->AddObserver(this);
  shell->display_manager()->AddDisplayManagerObserver(this);

  is_locked_ = shell->session_controller()->IsScreenLocked();

  RefreshState();
}

DisplayAlignmentController::~DisplayAlignmentController() {
  Shell* shell = Shell::Get();
  shell->display_manager()->RemoveDisplayManagerObserver(this);
  shell->session_controller()->RemoveObserver(this);
  shell->RemovePreTargetHandler(this);
}

void DisplayAlignmentController::OnDidApplyDisplayChanges() {
  RefreshState();
}

void DisplayAlignmentController::OnDisplaysInitialized() {
  RefreshState();
}

void DisplayAlignmentController::OnMouseEvent(ui::MouseEvent* event) {
  if (current_state_ == DisplayAlignmentState::kDisabled ||
      event->type() != ui::EventType::kMouseMoved) {
    return;
  }

  // If mouse enters the edge of the display.
  const gfx::Point screen_location = event->target()->GetScreenLocation(*event);

  const display::Display& src_display =
      Shell::Get()->display_manager()->FindDisplayContainingPoint(
          screen_location);

  if (!src_display.is_valid())
    return;

  const bool is_on_edge = IsOnBoundary(screen_location, src_display);

  // Restart the reset timer when the mouse moves off an edge.

  if (!is_on_edge) {
    if (current_state_ == DisplayAlignmentState::kOnEdge) {
      current_state_ = DisplayAlignmentState::kIdle;

      // The cursor was moved off the edge. Start the reset timer. If the cursor
      // does not hit an edge on the same display within |kCounterResetTime|,
      // state is reset by ResetState() and indicators will not be shown.
      action_trigger_timer_->Start(
          FROM_HERE, kCounterResetTime,
          base::BindOnce(&DisplayAlignmentController::ResetState,
                         base::Unretained(this)));
    }
    return;
  }

  if (current_state_ != DisplayAlignmentState::kIdle)
    return;

  // |trigger_count_| should only increment when the mouse hits the edges of
  // the same display.
  if (triggered_display_id_ == src_display.id()) {
    trigger_count_++;
  } else {
    triggered_display_id_ = src_display.id();
    trigger_count_ = 1;
  }

  action_trigger_timer_->Stop();
  current_state_ = DisplayAlignmentState::kOnEdge;

  if (trigger_count_ == kTriggerThresholdCount)
    ShowIndicators(src_display);
}

void DisplayAlignmentController::OnLockStateChanged(const bool locked) {
  is_locked_ = locked;
  RefreshState();
}

void DisplayAlignmentController::DisplayDragged(int64_t display_id,
                                                int32_t delta_x,
                                                int32_t delta_y) {
  if (current_state_ != DisplayAlignmentState::kLayoutPreview) {
    // Clear existing indicators. They are all regenerated via
    // OnDidApplyDisplayChanges() after dragging ends.
    ResetState();

    dragged_display_id_ = display_id;
    current_state_ = DisplayAlignmentState::kLayoutPreview;
  }

  // It is not possible to change display being dragged without dropping it
  // first (and causing update on display configuration).
  DCHECK_EQ(dragged_display_id_, display_id);
  DCHECK_NE(dragged_display_id_, display::kInvalidDisplayId);

  dragged_offset_ += gfx::Vector2d(delta_x, delta_y);

  ComputePreviewIndicators();
}

void DisplayAlignmentController::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  action_trigger_timer_ = std::move(timer);
}

const std::vector<std::unique_ptr<DisplayAlignmentIndicator>>&
DisplayAlignmentController::GetActiveIndicatorsForTesting() {
  return active_indicators_;
}

int64_t DisplayAlignmentController::GetDraggedDisplayIdForTesting() const {
  return dragged_display_id_;
}

void DisplayAlignmentController::ShowIndicators(
    const display::Display& src_display) {
  DCHECK_EQ(src_display.id(), triggered_display_id_);

  current_state_ = DisplayAlignmentState::kIndicatorsVisible;

  // Iterate through all the active displays and see if they are neighbors to
  // |src_display|.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const display::Displays& display_list =
      display_manager->active_display_list();
  for (const display::Display& peer : display_list) {
    // Skip currently triggered display or it might be detected as a neighbor
    if (peer.id() == triggered_display_id_)
      continue;

    // Check whether |src_display| and |peer| are neighbors.
    gfx::Rect source_edge;
    gfx::Rect peer_edge;
    if (display::ComputeBoundary(src_display, peer, &source_edge, &peer_edge)) {
      // TODO(1070697): Handle pills overlapping for certain display
      // configuration.

      // Pills are created for the indicators in the src display, but not in the
      // peers.
      const std::string& dst_name =
          display_manager->GetDisplayInfo(peer.id()).name();
      active_indicators_.push_back(DisplayAlignmentIndicator::CreateWithPill(
          src_display, source_edge, dst_name));

      active_indicators_.push_back(
          DisplayAlignmentIndicator::Create(peer, peer_edge));
    }
  }

  action_trigger_timer_->Start(
      FROM_HERE, kIndicatorVisibilityDuration,
      base::BindOnce(&DisplayAlignmentController::ResetState,
                     base::Unretained(this)));
}

void DisplayAlignmentController::ResetState() {
  action_trigger_timer_->Stop();
  active_indicators_.clear();
  trigger_count_ = 0;

  dragged_display_id_ = display::kInvalidDisplayId;
  dragged_offset_ = gfx::Vector2d(0, 0);

  // Do not re-enable if disabled.
  if (current_state_ != DisplayAlignmentState::kDisabled)
    current_state_ = DisplayAlignmentState::kIdle;
}

void DisplayAlignmentController::RefreshState() {
  ResetState();

  // This feature is only enabled when the screen is not locked and there is
  // more than one display connected.
  if (is_locked_) {
    current_state_ = DisplayAlignmentState::kDisabled;
    return;
  }

  const display::Displays& display_list =
      Shell::Get()->display_manager()->active_display_list();
  if (display_list.size() < 2) {
    current_state_ = DisplayAlignmentState::kDisabled;
    return;
  }

  if (current_state_ == DisplayAlignmentState::kDisabled)
    current_state_ = DisplayAlignmentState::kIdle;
}

void DisplayAlignmentController::ComputePreviewIndicators() {
  DCHECK_EQ(current_state_, DisplayAlignmentState::kLayoutPreview);
  DCHECK_NE(dragged_display_id_, display::kInvalidDisplayId);

  const display::Display& dragged_display =
      Shell::Get()->display_manager()->GetDisplayForId(dragged_display_id_);
  DCHECK(dragged_display.is_valid());

  gfx::Rect bounds = dragged_display.bounds();
  bounds += dragged_offset_;

  const display::Displays& display_list =
      Shell::Get()->display_manager()->active_display_list();

  // Iterate through all the active displays and see if they are neighbors to
  // |dragged_display|.
  for (const display::Display& peer : display_list) {
    // Skip currently dragged display or it might be detected as a neighbor
    if (peer.id() == dragged_display_id_)
      continue;

    // True if |source| and |peer| are neighbors. Returns |source_edge| and
    // |peer_edge| that denotes shared edges between |source| and |peer|
    // displays.
    gfx::Rect source_edge;
    gfx::Rect peer_edge;
    const bool are_neighbors = display::ComputeBoundary(
        bounds, peer.bounds(), &source_edge, &peer_edge);

    const auto& existing_indicator_it = base::ranges::find(
        active_indicators_, peer.id(), &DisplayAlignmentIndicator::display_id);

    const bool indicator_exists =
        existing_indicator_it != active_indicators_.end();

    if (indicator_exists) {
      if (are_neighbors) {
        // Displays are already neighbors.
        (*existing_indicator_it)->Update(peer, peer_edge);
        (*existing_indicator_it)->Show();
      } else {
        // Displays are no longer neighbors but previously were neighbors.
        (*existing_indicator_it)->Hide();
      }
    } else if (are_neighbors) {
      // Displays are newly-neighbored.
      active_indicators_.push_back(
          DisplayAlignmentIndicator::Create(peer, peer_edge));
    }
  }
}

}  // namespace ash
