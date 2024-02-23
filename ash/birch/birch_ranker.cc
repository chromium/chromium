// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_ranker.h"

#include <algorithm>

#include "ash/birch/birch_item.h"
#include "base/check.h"

namespace ash {

BirchRanker::BirchRanker(base::Time now) : now_(now) {}

BirchRanker::~BirchRanker() = default;

void BirchRanker::RankCalendarItems(std::vector<BirchCalendarItem>* items) {
  CHECK(items);
  // Sort the events by start time so that we can search forward in the vector
  // to find upcoming events.
  std::sort(items->begin(), items->end(),
            [](const BirchCalendarItem& a, const BirchCalendarItem& b) {
              return a.start_time < b.start_time;
            });

  const bool is_morning = IsMorning();
  const bool is_evening = IsEvening();
  bool found_upcoming_event = false;
  bool found_tomorrow_event = false;

  for (BirchCalendarItem& item : *items) {
    // Ongoing events have priority in the morning.
    if (is_morning && IsOngoingEvent(item)) {
      item.ranking = 6.f;
      continue;
    }

    // The first upcoming event has the same priority in the morning.
    if (is_morning && now_ < item.start_time && !found_upcoming_event) {
      found_upcoming_event = true;
      item.ranking = 6.f;
      continue;
    }

    // Ongoing events have medium priority all day.
    if (IsOngoingEvent(item)) {
      item.ranking = 9.f;
      continue;
    }

    // Events starting in the next 30 minutes has medium priority all day.
    if (now_ <= item.start_time && item.start_time < now_ + base::Minutes(30)) {
      item.ranking = 12.f;
      continue;
    }

    // In the evening, the first event from tomorrow has low priority.
    if (is_evening && IsTomorrowEvent(item) && !found_tomorrow_event) {
      found_tomorrow_event = true;
      item.ranking = 25.f;
      continue;
    }
  }
}

void BirchRanker::RankAttachmentItems(std::vector<BirchAttachmentItem>* items) {
  // TODO(b/305094126): Rank all data types.
}

void BirchRanker::RankFileSuggestItems(std::vector<BirchFileItem>* items) {
  // TODO(b/305094126): Rank all data types.
}

void BirchRanker::RankRecentTabItems(std::vector<BirchTabItem>* items) {
  // TODO(b/305094126): Rank all data types.
}

void BirchRanker::RankWeatherItems(std::vector<BirchWeatherItem>* items) {
  // In the morning, weather has high priority.
  const bool is_morning = IsMorning();
  if (is_morning && !items->empty()) {
    (*items)[0].ranking = 5.f;
  }

  // TODO(b/305094126): Figure out how to query the next day's weather and show
  // it in the evenings (8pm to midnight).
}

bool BirchRanker::IsMorning() const {
  base::Time last_midnight = now_.LocalMidnight();
  base::Time five_am_today = last_midnight + base::Hours(5);
  base::Time noon_today = last_midnight + base::Hours(12);
  return five_am_today <= now_ && now_ < noon_today;
}

bool BirchRanker::IsEvening() const {
  base::Time last_midnight = now_.LocalMidnight();
  base::Time five_pm_today = last_midnight + base::Hours(17);
  return five_pm_today <= now_;
}

bool BirchRanker::IsOngoingEvent(const BirchCalendarItem& item) const {
  return item.start_time <= now_ && now_ < item.end_time;
}

bool BirchRanker::IsTomorrowEvent(const BirchCalendarItem& item) const {
  return now_.LocalMidnight() + base::Days(1) < item.start_time;
}

}  // namespace ash
