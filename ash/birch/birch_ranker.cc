// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_ranker.h"

#include <algorithm>

#include "ash/birch/birch_item.h"
#include "base/check.h"
#include "base/time/time.h"

namespace {
// How long release notes remain top ranked.
constexpr base::TimeDelta kMinutesWhereReleaseNotesIsTopRanked =
    base::Minutes(10);
}  // namespace

namespace ash {

BirchRanker::BirchRanker(base::Time now) : now_(now) {}

BirchRanker::~BirchRanker() = default;

void BirchRanker::RankCalendarItems(std::vector<BirchCalendarItem>* items) {
  CHECK(items);
  // Sort the events by start time so that we can search forward in the vector
  // to find upcoming events.
  std::sort(items->begin(), items->end(),
            [](const BirchCalendarItem& a, const BirchCalendarItem& b) {
              return a.start_time() < b.start_time();
            });

  const bool is_morning = IsMorning();
  const bool is_evening = IsEvening();
  bool found_upcoming_event = false;
  bool found_tomorrow_event = false;

  for (BirchCalendarItem& item : *items) {
    // Ongoing events have priority in the morning.
    if (is_morning && IsOngoingEvent(item)) {
      item.set_ranking(6.f);
      continue;
    }

    // The first upcoming event has the same priority in the morning.
    if (is_morning && now_ < item.start_time() && !found_upcoming_event) {
      found_upcoming_event = true;
      item.set_ranking(6.f);
      continue;
    }

    // Ongoing events have medium priority all day.
    if (IsOngoingEvent(item)) {
      item.set_ranking(9.f);
      continue;
    }

    // Events starting in the next 30 minutes has medium priority all day.
    if (now_ <= item.start_time() &&
        item.start_time() < now_ + base::Minutes(30)) {
      item.set_ranking(12.f);
      continue;
    }

    // In the evening, the first event from tomorrow has low priority.
    if (is_evening && IsTomorrowEvent(item) && !found_tomorrow_event) {
      found_tomorrow_event = true;
      item.set_ranking(25.f);
      continue;
    }
  }
}

void BirchRanker::RankAttachmentItems(std::vector<BirchAttachmentItem>* items) {
  CHECK(items);

  // Sort the attachments by their event start time.
  std::sort(items->begin(), items->end(),
            [](const BirchAttachmentItem& a, const BirchAttachmentItem& b) {
              return a.start_time() < b.start_time();
            });

  const bool is_morning = IsMorning();

  for (BirchAttachmentItem& item : *items) {
    // Attachments for ongoing events have high priority in the morning and
    // medium priority the rest of the day.
    const bool is_ongoing = item.start_time() <= now_ && now_ < item.end_time();
    if (is_ongoing) {
      item.set_ranking(is_morning ? 7.f : 10.f);
      continue;
    }

    // Attachments for events starting in the next 30 minutes have medium
    // priority.
    if (now_ <= item.start_time() &&
        item.start_time() < now_ + base::Minutes(30)) {
      item.set_ranking(13.f);
      continue;
    }
  }
}

void BirchRanker::RankFileSuggestItems(std::vector<BirchFileItem>* items) {
  CHECK(items);

  // Sort the file suggestions by their timestamp, descending.
  std::sort(items->begin(), items->end(),
            [](const BirchFileItem& a, const BirchFileItem& b) {
              return b.timestamp() < a.timestamp();
            });

  // TODO(b/305094126): Differentiate between modify time and share time.
  // Currently the single timestamp represents both.
  for (BirchFileItem& item : *items) {
    // Items modified/shared recently have high priority.
    if (now_ - base::Hours(1) < item.timestamp()) {
      item.set_ranking(19.f);
      continue;
    }
    // Items modified/shared today have medium priority.
    if (now_ - base::Days(1) < item.timestamp()) {
      item.set_ranking(32.f);
      continue;
    }
    // Items modified/shared this week have low priority.
    if (now_ - base::Days(7) < item.timestamp()) {
      item.set_ranking(40.f);
      continue;
    }
  }
}

void BirchRanker::RankRecentTabItems(std::vector<BirchTabItem>* items) {
  CHECK(items);

  // Sort the recent tabs by their timestamp, descending.
  std::sort(items->begin(), items->end(),
            [](const BirchTabItem& a, const BirchTabItem& b) {
              return b.timestamp() < a.timestamp();
            });

  // TODO(b/305094126): Distinguish between tabs from mobile and tabs from
  // desktop.
  for (BirchTabItem& item : *items) {
    // Very recent items have high priority.
    if (now_ - base::Minutes(5) < item.timestamp()) {
      item.set_ranking(14.f);
      continue;
    }
    // Items from the last hour have medium priority.
    if (now_ - base::Hours(1) < item.timestamp()) {
      item.set_ranking(17.f);
      continue;
    }
    // Items from the last day have low priority.
    if (now_ - base::Days(1) < item.timestamp()) {
      item.set_ranking(30.f);
      continue;
    }
  }
}

void BirchRanker::RankWeatherItems(std::vector<BirchWeatherItem>* items) {
  // In the morning, weather has high priority.
  const bool is_morning = IsMorning();
  if (is_morning && !items->empty()) {
    (*items)[0].set_ranking(5.f);
  }

  // TODO(b/305094126): Figure out how to query the next day's weather and show
  // it in the evenings (8pm to midnight).
}

void BirchRanker::RankReleaseNotesItems(
    std::vector<BirchReleaseNotesItem>* items) {
  for (BirchReleaseNotesItem& item : *items) {
    item.set_ranking(GetReleaseNotesItemRanking(item));
  }
}

float BirchRanker::GetReleaseNotesItemRanking(
    const BirchReleaseNotesItem& item) const {
  const base::TimeDelta elapsed_time = now_ - item.first_seen();
  if (elapsed_time <= kMinutesWhereReleaseNotesIsTopRanked) {
    return 3.0f;
  }
  if (elapsed_time <= base::Hours(1)) {
    return 13.0f;
  }
  if (elapsed_time <= base::Hours(24)) {
    return 26.0f;
  }
  return 42.0f;
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
  return item.start_time() <= now_ && now_ < item.end_time();
}

bool BirchRanker::IsTomorrowEvent(const BirchCalendarItem& item) const {
  return now_.LocalMidnight() + base::Days(1) < item.start_time();
}

}  // namespace ash
