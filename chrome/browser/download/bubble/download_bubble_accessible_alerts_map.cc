// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_accessible_alerts_map.h"

#include <map>
#include <string>
#include <vector>

namespace {
// Only alert about a given download if it has been at least 3 minutes since
// the last alert for that download.
constexpr base::TimeDelta kAlertInterval = base::Minutes(3);
// Discard an alert if it has been more than 1 minute since it was added, even
// if it has never been announced. After this time interval, we consider it
// too stale to announce.
constexpr base::TimeDelta kAlertMaxStaleness = base::Minutes(1);
// Only track last-alerted times for 6 minutes. If no one attempts to
// re-announce an alert for a download within 6 minutes, we can garbage collect
// the last-alerted time and it will be as if that download was never announced.
constexpr base::TimeDelta kTrackAlertedTimesThreshold = base::Minutes(6);

using offline_items_collection::ContentId;
}  // namespace

DownloadBubbleAccessibleAlertsMap::Alert::Alert(Urgency urgency,
                                                std::u16string_view alert_text)
    : urgency(urgency),
      added_time(base::Time::Now()),
      text(std::u16string(alert_text)) {}

DownloadBubbleAccessibleAlertsMap::Alert::Alert(const Alert& other) = default;

DownloadBubbleAccessibleAlertsMap::Alert&
DownloadBubbleAccessibleAlertsMap::Alert::operator=(const Alert& other) =
    default;

DownloadBubbleAccessibleAlertsMap::Alert::Alert(Alert&& other) = default;

DownloadBubbleAccessibleAlertsMap::Alert&
DownloadBubbleAccessibleAlertsMap::Alert::operator=(Alert&& other) = default;

bool DownloadBubbleAccessibleAlertsMap::Alert::ShouldBeReplacedBy(
    const DownloadBubbleAccessibleAlertsMap::Alert& newer) const {
  return newer.added_time >= added_time && newer.urgency <= urgency;
}

bool DownloadBubbleAccessibleAlertsMap::Alert::IsStale() const {
  return base::Time::Now() - added_time > kAlertMaxStaleness;
}

DownloadBubbleAccessibleAlertsMap::DownloadBubbleAccessibleAlertsMap() =
    default;

DownloadBubbleAccessibleAlertsMap::~DownloadBubbleAccessibleAlertsMap() =
    default;

bool DownloadBubbleAccessibleAlertsMap::MaybeAddAccessibleAlert(
    const offline_items_collection::ContentId& content_id,
    Alert alert) {
  if (alert.text.empty()) {
    return false;
  }

  if (auto it = unannounced_alerts_.find(content_id);
      it != unannounced_alerts_.end()) {
    if (it->second.ShouldBeReplacedBy(alert) || it->second.IsStale()) {
      it->second = std::move(alert);
      return true;
    }
    return false;
  }
  return unannounced_alerts_.emplace(content_id, std::move(alert)).second;
}

std::vector<std::u16string>
DownloadBubbleAccessibleAlertsMap::TakeAlertsForAnnouncement() {
  std::vector<std::u16string> to_announce;
  base::Time now = base::Time::Now();

  // Iterate over the two maps together.
  for (struct {
         AlertsMap::iterator alerts_it;
         LastAlertedTimesMap::iterator times_it;
       } its = {unannounced_alerts_.begin(), last_alerted_times_.begin()};
       its.alerts_it != unannounced_alerts_.end();) {
    const ContentId& alert_content_id = its.alerts_it->first;

    // Move the iterator to the position of or just past the
    // last-alerted-time for this content_id.
    while (its.times_it != last_alerted_times_.end() &&
           its.times_it->first < alert_content_id) {
      ++its.times_it;
    }
    bool has_previous_announcement =
        its.times_it != last_alerted_times_.end() &&
        its.times_it->first == alert_content_id;

    // If the alert is stale, don't announce it. If the alert is kAlertSoon,
    // definitely announce it. If the alert is kAlertWhenAppropriate, only
    // announce it if we have not previously announced for this download, or if
    // the last announcement for this download was long enough in the past.
    if (!its.alerts_it->second.IsStale() &&
        (its.alerts_it->second.urgency == Alert::Urgency::kAlertSoon ||
         !has_previous_announcement ||
         now - its.times_it->second > kAlertInterval)) {
      to_announce.emplace_back(std::move(its.alerts_it->second.text));
      its.times_it = last_alerted_times_.insert_or_assign(
          its.times_it, alert_content_id, now);
      its.alerts_it = unannounced_alerts_.erase(its.alerts_it);
    } else if (its.alerts_it->second.IsStale()) {
      its.alerts_it = unannounced_alerts_.erase(its.alerts_it);
    } else {
      ++its.alerts_it;
    }
  }
  return to_announce;
}

void DownloadBubbleAccessibleAlertsMap::GarbageCollect() {
  std::erase_if(unannounced_alerts_,
                [](const auto& kv) { return kv.second.IsStale(); });
  base::Time now = base::Time::Now();
  std::erase_if(last_alerted_times_, [now](const auto& kv) {
    return now - kv.second > kTrackAlertedTimesThreshold;
  });
}
