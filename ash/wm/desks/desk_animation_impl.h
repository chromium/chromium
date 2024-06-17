// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
#define ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ui {
class PresentationTimeRecorder;
}

namespace ash {

class ASH_EXPORT DeskActivationAnimation : public DeskAnimationBase {
 public:
  DeskActivationAnimation(DesksController* controller,
                          int starting_desk_index,
                          int ending_desk_index,
                          DesksSwitchSource source,
                          bool update_window_activation);
  DeskActivationAnimation(const DeskActivationAnimation&) = delete;
  DeskActivationAnimation& operator=(const DeskActivationAnimation&) = delete;
  ~DeskActivationAnimation() override;

  // DeskAnimationBase:
  bool Replace(bool moving_left, DesksSwitchSource source) override;
  bool UpdateSwipeAnimation(float scroll_delta_x) override;
  bool EndSwipeAnimation() override;
  bool CanEnterOverview() const override;
  void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) override;
  void OnDeskSwitchAnimationFinishedInternal() override;
  LatencyReportCallback GetLatencyReportCallback() const override;
  metrics_util::ReportCallback GetSmoothnessReportCallback() const override;

  void AddOnAnimationFinishedCallbackForTesting(base::OnceClosure callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(DeskActivationAnimationTest,
                           AnimatingAfterFastSwipe);
  FRIEND_TEST_ALL_PREFIXES(OverviewDeskNavigationTest,
                           ShortSwipeStaysInOverview);

  // Prepares the desk associated with |index| for taking a screenshot. Exits
  // overview and splitview if necessary and then activates the desk. Restores
  // splitview or overview if necessary after activating the desk.
  void PrepareDeskForScreenshot(int index);

  // The switch source that requested this animation.
  const DesksSwitchSource switch_source_;

  // True if we should pass window activation to a window on the target desk
  // when the desk is switched.
  const bool update_window_activation_;

  // The index of the desk that is most visible to the user based on the
  // transform of the animation layer.
  int visible_desk_index_;

  // The last time an animation has been started or replaced. This is used to
  // help determine which desk to animate to when EndSwipeAnimation is called.
  base::TimeTicks last_start_or_replace_time_;

  // Used to measure the presentation time of a continuous gesture swipe.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Callback that is run after the animation is finished for testing purposes.
  base::OnceClosure on_animation_finished_callback_for_testing_;

  base::WeakPtrFactory<DeskActivationAnimation> weak_ptr_factory_{this};
};

class DeskRemovalAnimation : public DeskAnimationBase {
 public:
  DeskRemovalAnimation(DesksController* controller,
                       int desk_to_remove_index,
                       int desk_to_activate_index,
                       DesksCreationRemovalSource source,
                       DeskCloseType close_type);
  DeskRemovalAnimation(const DeskRemovalAnimation&) = delete;
  DeskRemovalAnimation& operator=(const DeskRemovalAnimation&) = delete;
  ~DeskRemovalAnimation() override;

  // DeskAnimationBase:
  void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) override;
  void OnDeskSwitchAnimationFinishedInternal() override;
  LatencyReportCallback GetLatencyReportCallback() const override;
  metrics_util::ReportCallback GetSmoothnessReportCallback() const override;

 private:
  const int desk_to_remove_index_;
  const DesksCreationRemovalSource request_source_;
  const DeskCloseType close_type_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
