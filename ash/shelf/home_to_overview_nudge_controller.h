// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_TO_OVERVIEW_NUDGE_CONTROLLER_H_
#define ASH_SHELF_HOME_TO_OVERVIEW_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class ContextualNudge;
class HotseatWidget;

class ASH_EXPORT HomeToOverviewNudgeController : views::WidgetObserver {
 public:
  // Indicates the reason the nudge is being hidden. Used to determine the exact
  // transition to use when hiding the nudge.
  enum class HideTransition { kShelfStateChange, kUserTap, kNudgeTimeout };

  explicit HomeToOverviewNudgeController(HotseatWidget* hotseat_widget);
  HomeToOverviewNudgeController(const HomeToOverviewNudgeController& other) =
      delete;
  ~HomeToOverviewNudgeController() override;

  HomeToOverviewNudgeController& operator=(
      const HomeToOverviewNudgeController& other) = delete;

  // Sets whether the home to overview nudge can be shown for the current shelf
  // state. If nudge is allowed, controller may show the nudge (if required). If
  // the nudge is not allowed, the nudge will be hidden if currently shown.
  void SetNudgeAllowedForCurrentShelf(bool allowed);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  ContextualNudge* nudge_for_testing() { return nudge_; }

  bool HasShowTimerForTesting() const;
  void FireShowTimerForTesting();

  bool HasHideTimerForTesting() const;
  void FireHideTimerForTesting();

 private:
  // Creates and shows the nudge bubble, schedules showing animation for the
  // nudge and hotseat widgets, and schedules nudge hide timer as needed.
  void ShowNudge();

  // Sets up hotseat and nudge wdget animation for hiding the nudge, and closes
  // the nudge widget when the animation finishes.
  void HideNudge(HideTransition transition);

  // Updates the nudge anchor bounds for the current hotseat and shelf bounds.
  void UpdateNudgeAnchorBounds();

  // Passed to |nudge_| as its tap gesture handler.
  void HandleNudgeTap();

  bool nudge_allowed_for_shelf_state_ = false;

  const raw_ptr<HotseatWidget, DanglingUntriaged> hotseat_widget_;
  raw_ptr<ContextualNudge> nudge_ = nullptr;

  base::OneShotTimer nudge_show_timer_;
  base::OneShotTimer nudge_hide_timer_;

  // Observes hotseat widget to detect the hotseat bounds changes, and the
  // nudge widget to detect that the widget is being destroyed.
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};

  base::WeakPtrFactory<HomeToOverviewNudgeController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_TO_OVERVIEW_NUDGE_CONTROLLER_H_
