// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
#define ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_

#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desks_histogram_enums.h"

namespace ash {

class DesksController;
class PresentationTimeRecorder;

class DeskActivationAnimation : public DeskAnimationBase {
 public:
  DeskActivationAnimation(DesksController* controller,
                          int starting_desk_index,
                          int ending_desk_index,
                          DesksSwitchSource source);
  DeskActivationAnimation(const DeskActivationAnimation&) = delete;
  DeskActivationAnimation& operator=(const DeskActivationAnimation&) = delete;
  ~DeskActivationAnimation() override;

  // DeskAnimationBase:
  bool Replace(bool moving_left, DesksSwitchSource source) override;
  bool UpdateSwipeAnimation(float scroll_delta_x) override;
  bool EndSwipeAnimation() override;
  void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) override;
  void OnDeskSwitchAnimationFinishedInternal() override;
  metrics_util::ReportCallback GetReportCallback() const override;

 private:
  // Prepares the desk associated with |index| for taking a screenshot. Exits
  // overview and splitview if necessary and then activates the desk. Restores
  // splitview if necessary after activating the desk.
  void PrepareDeskForScreenshot(int index);

  // The switch source that requested this animation.
  const DesksSwitchSource switch_source_;

  // Used to measure the presentation time of a continuous gesture swipe.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;
};

class DeskRemovalAnimation : public DeskAnimationBase {
 public:
  DeskRemovalAnimation(DesksController* controller,
                       int desk_to_remove_index,
                       int desk_to_activate_index,
                       DesksCreationRemovalSource source);
  DeskRemovalAnimation(const DeskRemovalAnimation&) = delete;
  DeskRemovalAnimation& operator=(const DeskRemovalAnimation&) = delete;
  ~DeskRemovalAnimation() override;

  // DeskAnimationBase:
  void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) override;
  void OnDeskSwitchAnimationFinishedInternal() override;
  metrics_util::ReportCallback GetReportCallback() const override;

 private:
  const int desk_to_remove_index_;
  const DesksCreationRemovalSource request_source_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
