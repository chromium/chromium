// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_DAILY_NAVIGATION_COUNTER_H_
#define CHROME_BROWSER_SSL_DAILY_NAVIGATION_COUNTER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"

namespace base {
class Clock;
}

// This class records the number of navigations inside a rolling window. Each
// navigation is counted in its day's bucket. Counts are regularly saved to
// a dictionary which can be backed by a pref, so that the counts are persisted.
// Counts belonging to the days outside the rolling window are discarded.
// This class is only intended to count an approximate the number of
// navigations for HTTPS-First Mode related heuristics. Browser restarts or
// crashes may lose some counts which shouldn't affect the heuristics
// significantly.
class DailyNavigationCounter {
 public:
  // `dict` is the backing dictionary where counts are read from and saved to.
  // `clock` is the clock used to get time.
  // `rolling_window_duration_in_days` is the size of the rolling window during
  // which navigations are recorded. Counts outside this window will be
  // discarded. E.g. rolling_window_duration_in_days=1 means counts added two
  // days ago will be discarded today.
  // `save_interval` is the number of navigations after which the counts are
  // saved to `dict`.
  DailyNavigationCounter(base::Value::Dict* dict,
                         base::Clock* clock,
                         size_t rolling_window_duration_in_days,
                         size_t save_interval);
  virtual ~DailyNavigationCounter();

  // Returns the total number of navigations.
  size_t GetTotal() const;
  // Increments the number of navigations for this day. Returns true if the
  // counts are written to `dict`.
  bool Increment();

  // Returns the number of navigations that are in memory but not yet saved
  // to the dict.
  size_t unsaved_count_for_testing() const { return unsaved_count_; }

 private:
  raw_ptr<base::Value::Dict> dict_;
  raw_ptr<base::Clock> clock_;
  // Number of navigations that haven't been saved in the pref yet.
  int unsaved_count_ = 0;
  // Age of the oldest navigation to be counted. Anything older than this will
  // be discarded.
  size_t rolling_window_duration_in_days_;
  // If the number of unsaved navigations is larger than this, counts will be
  // saved to the dict.
  int save_interval_ = 10;
  // The map that counts the navigations. Keys are dates formatted as
  // yyyy-MM-DD, values are the number of navigations on that day. At any point,
  // there will be at most (save_interval_ + rolling_window_duration_in_days)
  // different keys in this map. After that, entries will be written into
  // dict_, discarding old entries.
  //
  // This could be a simple integer, but that would would prevent us from
  // discarding very old counts when the profile wasn't used for a long time.
  // E.g. 99 navigations made 20 days ago, then zero for 20 days, then one
  // navigation next day would return 100 navigations in total instead of one,
  // if save_interval_=100.
  base::flat_map<std::string, int> counts_map_;
};

#endif  // CHROME_BROWSER_SSL_DAILY_NAVIGATION_COUNTER_H_
