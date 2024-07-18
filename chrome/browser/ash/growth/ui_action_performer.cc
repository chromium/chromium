// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/ui_action_performer.h"

#include "chrome/browser/ash/growth/metrics.h"

UiActionPerformer::UiActionPerformer() = default;

UiActionPerformer::~UiActionPerformer() = default;

void UiActionPerformer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UiActionPerformer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UiActionPerformer::NotifyReadyToLogImpression(
    int campaign_id,
    std::optional<int> group_id,
    bool should_log_cros_events) {
  for (auto& observer : observers_) {
    observer.OnReadyToLogImpression(campaign_id, group_id,
                                    should_log_cros_events);
  }
}

void UiActionPerformer::NotifyDismissed(int campaign_id,
                                        std::optional<int> group_id,
                                        bool should_mark_dismissed,
                                        bool should_log_cros_events) {
  for (auto& observer : observers_) {
    observer.OnDismissed(campaign_id, group_id, should_mark_dismissed,
                         should_log_cros_events);
  }
}

void UiActionPerformer::NotifyButtonPressed(int campaign_id,
                                            std::optional<int> group_id,
                                            CampaignButtonId button_id,
                                            bool should_mark_dismissed,
                                            bool should_log_cros_events) {
  for (auto& observer : observers_) {
    observer.OnButtonPressed(campaign_id, group_id, button_id,
                             should_mark_dismissed, should_log_cros_events);
  }
}
