// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ACCESSIBLE_ALERTS_MAP_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ACCESSIBLE_ALERTS_MAP_H_

#include <compare>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "components/offline_items_collection/core/offline_item.h"

// Holds accessible alerts to be announced by the download bubble.
// Manages the lifecycle of alerts including suppressing alerts for a download
// that has already recently been alerted about.
class DownloadBubbleAccessibleAlertsMap {
 public:
  struct Alert {
    // Describes the urgency with which an alert should be announced.
    // Values should be sorted in order of most urgent first.
    enum class Urgency {
      // An urgent alert, which should be announced at the next available
      // opportunity.
      kAlertSoon,

      // An alert than can wait for the next appropriate time, e.g. based on a
      // per-download timer.
      kAlertWhenAppropriate,
    };

    // Sets `added_time` to now.
    Alert(Urgency urgency, std::u16string_view alert_text);

    // Movable and copyable.
    Alert(const Alert& other);
    Alert& operator=(const Alert& other);
    Alert(Alert&& other);
    Alert& operator=(Alert&& other);

    Urgency urgency = Urgency::kAlertWhenAppropriate;

    // The time when this alert was generated, due to a download update. Not
    // necessarily the time when it was announced to the user.
    base::Time added_time;

    std::u16string text;

    // Whether `newer` should overwrite `this`, based on urgency and added time,
    // assuming `this` is an existing alert in the map and `newer` is about to
    // be newly added. The `added_time` of `newer` must be at least as recent as
    // that of `this` (but this is not enforced as a CHECK because system clocks
    // may jump backwards sometimes).
    bool ShouldBeReplacedBy(const Alert& newer) const;

    // Whether it has been too long since the alert was added, such that it
    // should no longer be announced.
    bool IsStale() const;

    auto operator<=>(const Alert&) const = default;
  };

  using AlertsMap = std::map<offline_items_collection::ContentId, Alert>;
  using LastAlertedTimesMap =
      std::map<offline_items_collection::ContentId, base::Time>;

  DownloadBubbleAccessibleAlertsMap();

  DownloadBubbleAccessibleAlertsMap(
      const DownloadBubbleAccessibleAlertsMap& other) = delete;
  DownloadBubbleAccessibleAlertsMap& operator=(
      const DownloadBubbleAccessibleAlertsMap& other) = delete;

  ~DownloadBubbleAccessibleAlertsMap();

  // Maybe adds a non-empty accessible alert to the map.  Returns whether alert
  // was added. If an alert for the download identified by `content_id` already
  // exists, it may be overwritten by the new alert depending on urgency and
  // time. If the alert text is empty, it is not added.
  bool MaybeAddAccessibleAlert(
      const offline_items_collection::ContentId& content_id,
      Alert alert);

  // Exports the list of unannounced alerts eligible for announcement. They are
  // removed from `unannounced_alerts_`.
  std::vector<std::u16string> TakeAlertsForAnnouncement();

  // Iterates over both maps and garbage collects stale alerts and
  // last-alerted times that were too long ago.
  void GarbageCollect();

  const AlertsMap& unannounced_alerts_for_testing() {
    return unannounced_alerts_;
  }

  const LastAlertedTimesMap& last_alerted_times_for_testing() {
    return last_alerted_times_;
  }

 private:
  // Maps content id to Alert. Holds only alerts that have built up since the
  // last call to TakeAlertsForAnnouncement.
  AlertsMap unannounced_alerts_;

  // Maps content id to last time an alert was announced for a given download.
  LastAlertedTimesMap last_alerted_times_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ACCESSIBLE_ALERTS_MAP_H_
