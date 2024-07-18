// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_UI_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_UI_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chromeos/ash/components/growth/action_performer.h"

// Implements a base action performer to show visial elements, such a nudge and
// a notification.
class UiActionPerformer : public growth::ActionPerformer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Trigger when calling to show the UI. The UI may or may not show.
    virtual void OnReadyToLogImpression(int campaign_id,
                                        std::optional<int> group_id,
                                        bool should_log_cros_events) = 0;

    // Trigger when the UI is pressed.
    // NOTE: Any button press could dismiss the UI. And the UI could auto
    // dismiss after some time.
    // TODO: b/330956316 - Log dismissal by reasons, e.g. the nudge is dismissed
    // automatically.
    virtual void OnDismissed(int campaign_id,
                             std::optional<int> group_id,
                             bool should_mark_dismissed,
                             bool should_log_cros_events) = 0;

    // Trigger when the button in the UI (if exists) is pressed.
    virtual void OnButtonPressed(int campaign_id,
                                 std::optional<int> group_id,
                                 CampaignButtonId button_id,
                                 bool should_mark_dismissed,
                                 bool should_log_cros_events) = 0;
  };

  UiActionPerformer();
  ~UiActionPerformer() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyReadyToLogImpression(int campaign_id,
                                  std::optional<int> group_id,
                                  bool should_log_cros_events = false);
  void NotifyDismissed(int campaign_id,
                       std::optional<int> group_id,
                       bool should_mark_dismissed,
                       bool should_log_cros_events = false);
  void NotifyButtonPressed(int campaign_id,
                           std::optional<int> group_id,
                           CampaignButtonId button_id,
                           bool should_mark_dismissed,
                           bool should_log_cros_events = false);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_UI_ACTION_PERFORMER_H_
