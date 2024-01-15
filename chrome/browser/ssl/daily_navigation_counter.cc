// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/daily_navigation_counter.h"

#include "base/i18n/time_formatting.h"
#include "base/time/clock.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace {

// Returns the given time in yyyy-MM-dd format.
std::string GetDateString(base::Time now) {
  return base::UnlocalizedTimeFormatWithPattern(now.UTCMidnight(), "yyyy-MM-dd",
                                                icu::TimeZone::getGMT());
}

}  // namespace

DailyNavigationCounter::DailyNavigationCounter(
    base::Value::Dict* dict,
    base::Clock* clock,
    size_t rolling_window_duration_in_days,
    size_t save_interval)
    : dict_(dict),
      clock_(clock),
      rolling_window_duration_in_days_(rolling_window_duration_in_days),
      save_interval_(save_interval) {
  DCHECK(save_interval_ > 0);

  // Load from the saved dict into the current map, discarding old entries.
  base::Time now = clock_->Now();
  base::Time cutoff =
      now.UTCMidnight() - base::Days(rolling_window_duration_in_days_);

  for (auto [key, value] : *dict_) {
    base::Time timestamp;
    if (!base::Time::FromUTCString(key.c_str(), &timestamp)) {
      // Not a valid bucket.
      continue;
    }
    if (timestamp < cutoff) {
      // Old bucket.
      continue;
    }
    std::optional<int> count = dict->FindInt(key);
    if (!count.has_value()) {
      continue;
    }
    counts_map_[key] = count.value();
  }
}

DailyNavigationCounter::~DailyNavigationCounter() = default;

size_t DailyNavigationCounter::GetTotal() const {
  size_t total = 0;
  for (auto [key, value] : counts_map_) {
    total += value;
  }
  return total;
}

bool DailyNavigationCounter::Increment() {
  base::Time now = clock_->Now();
  std::string today = GetDateString(now);
  counts_map_[today]++;
  unsaved_count_++;
  if (unsaved_count_ < save_interval_) {
    // Not stale.
    return false;
  }

  // Otherwise, discard old buckets and save the current map to the backing
  // dict.
  base::Time cutoff =
      now.UTCMidnight() - base::Days(rolling_window_duration_in_days_);
  base::flat_map<std::string, int> new_map_;
  dict_->clear();
  for (auto [key, value] : counts_map_) {
    base::Time timestamp;
    if (!base::Time::FromUTCString(key.c_str(), &timestamp)) {
      // Not a valid bucket.
      continue;
    }
    if (timestamp < cutoff) {
      // Old bucket.
      continue;
    }
    new_map_[key] = value;
    dict_->Set(key, value);
  }
  counts_map_ = new_map_;
  unsaved_count_ = 0;
  return true;
}
