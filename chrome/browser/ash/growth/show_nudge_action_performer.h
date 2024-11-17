// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
}

// Dictionary of supported nudge payload. For example:
// {
//   "title": "Nudge title",
//   "body": "Body text"
// }
using NudgePayload = base::Value::Dict;

// Implements the action to show nudge.
class ShowNudgeActionPerformer : public UiActionPerformer,
                                 views::WidgetObserver {
 public:
  ShowNudgeActionPerformer();
  ~ShowNudgeActionPerformer() override;

  // growth::Action:
  void Run(int campaign_id,
           std::optional<int> group_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;

  void SetAnchoredViewForTesting(
      std::optional<views::View*> anchored_view_for_test);

 private:
  bool ShowNudge(int campaign_id,
                 std::optional<int> group_id,
                 const NudgePayload* nudge_payload);
  bool MaybeSetAnchorView(const base::Value::Dict* anchor_dict,
                          ash::AnchoredNudgeData& nudge_data);
  void MaybeSetButtonData(int campaign_id,
                          std::optional<int> group_id,
                          const base::Value::Dict* button_dict,
                          ash::AnchoredNudgeData& nudge_data,
                          bool is_primary,
                          bool should_log_cros_events);
  void OnNudgeButtonClicked(int campaign_id,
                            std::optional<int> group_id,
                            CampaignButtonId button_id,
                            const base::Value::Dict* action_dict,
                            bool should_mark_dismissed,
                            bool should_log_cros_events);
  void OnNudgeDismissed(int campaign_id,
                        std::optional<int> group_id,
                        bool should_log_cros_events);
  void MaybeSetWidgetObservers();
  void MaybeCancelNudge();
  void CancelNudge();

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  raw_ptr<views::Widget> triggering_widget_ = nullptr;
  bool is_nudge_active_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      nudge_widget_scoped_observation_{this};

  base::WeakPtrFactory<ShowNudgeActionPerformer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
