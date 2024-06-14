// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/vector2d.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

class DisplayAlignmentIndicator;

// DisplayAlignmentController is responsible for creating new
// DisplayAlignmentIndicators when the activation criteria is met.
// TODO(1091497): Consider combining DisplayHighlightController and
// DisplayAlignmentController.
class ASH_EXPORT DisplayAlignmentController
    : public ui::EventHandler,
      public display::DisplayManagerObserver,
      public SessionObserver {
 public:
  enum class DisplayAlignmentState {
    // No indicators shown and mouse is not on edge
    kIdle,

    // Mouse is currently on one of the edges.
    kOnEdge,

    // The indicators are visible.
    kIndicatorsVisible,

    // A display is being dragged around in display layouts. Preview indicators
    // are being updated and shown.
    kLayoutPreview,

    // Screen is locked or there is only one display.
    kDisabled,
  };

  DisplayAlignmentController();
  DisplayAlignmentController(const DisplayAlignmentController&) = delete;
  DisplayAlignmentController& operator=(const DisplayAlignmentController&) =
      delete;
  ~DisplayAlignmentController() override;

  // display::DisplayManagerObserver
  void OnDidApplyDisplayChanges() override;
  void OnDisplaysInitialized() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Update positions of display alignment preview highlights. Display being
  // dragged is specified by |display_id|. |preview_indicators_| is
  // populated with indicators from this display and its neighbors as
  // it is not possible for |display_id| to change mid-drag.
  void DisplayDragged(int64_t display_id, int32_t delta_x, int32_t delta_y);

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  const std::vector<std::unique_ptr<DisplayAlignmentIndicator>>&
  GetActiveIndicatorsForTesting();

  int64_t GetDraggedDisplayIdForTesting() const;

 private:
  // Show all indicators on |src_display| and other indicators that shares
  // an edge with |src_display|. Indicators on other displays are shown without
  // pills. All indicators are created in this method and stored in
  // |active_indicators_| to be destroyed in ResetState().
  void ShowIndicators(const display::Display& src_display);

  // Clears all indicators, containers, timer, and resets the state back to
  // kIdle.
  void ResetState();

  // Used to transition to kDisable if required. Called whenever display
  // configuration or lock state updates.
  void RefreshState();

  // Updates, shows/hides preview indicators according to changes reported by
  // DisplayDragged().
  void ComputePreviewIndicators();

  // Stores all DisplayAlignmentIndicators currently being shown. All indicators
  // should either belong to or be a shared edge of display with
  // |triggered_display_id_|. Indicators are created upon activation in
  // ShowIndicators() or upon adjusting display layout in
  // ComputePreviewIndicators() and cleared in ResetState().
  std::vector<std::unique_ptr<DisplayAlignmentIndicator>> active_indicators_;

  // Timer used for both edge trigger timeouts and hiding indicators.
  std::unique_ptr<base::OneShotTimer> action_trigger_timer_;

  // Tracks current state of the controller. Mostly used to determine if action
  // is taken in OnMouseEvent();
  DisplayAlignmentState current_state_ = DisplayAlignmentState::kIdle;

  // Tracks if the screen is locked to disable highlights.
  bool is_locked_ = false;

  // Keeps track of the most recent display where the mouse hit the edge.
  // Prevents activating indicators when user hits edges of different displays.
  int64_t triggered_display_id_ = display::kInvalidDisplayId;

  // Number of times the mouse was on an edge of some display specified by
  // |triggered_display_id_| recently.
  int trigger_count_ = 0;

  // ID of display currently beign dragged. Cannot change from one valid
  // ID to another as dropping the dragged display causes changes to display
  // configuration, resetting this ID.
  int64_t dragged_display_id_;

  // The difference between dragged display's actual position and preview
  // position. Is an accumulation of |delta_x| and |delta_y| from
  // DisplayDragged(). The offset is reset when a configuration event occurs.
  gfx::Vector2d dragged_offset_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_
