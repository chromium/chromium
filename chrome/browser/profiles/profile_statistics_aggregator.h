// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

class ProfileStatisticsAggregator {
  // This class is used internally by ProfileStatistics
  //
  // The class collects statistical information about the profile and returns
  // the information via a callback function. Currently bookmarks, history,
  // logins and sites with autofill forms are counted.

 public:
  ProfileStatisticsAggregator(Profile* profile,
                              base::OnceClosure done_callback);
  ProfileStatisticsAggregator(const ProfileStatisticsAggregator&) = delete;
  ProfileStatisticsAggregator& operator=(const ProfileStatisticsAggregator&) =
      delete;
  ~ProfileStatisticsAggregator();

  void AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback stats_callback);

 private:
  // Start gathering statistics. Also cancels existing statistics tasks.
  void StartAggregator();

  // Callback functions
  // Appends result to |profile_category_stats_|, and then calls
  // the external callback.
  void StatisticsCallback(const char* category, int count);

  // Callback for counters.
  void OnCounterResult(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  // Registers, initializes and starts a BrowsingDataCounter.
  void AddCounter(std::unique_ptr<browsing_data::BrowsingDataCounter> counter);

  raw_ptr<Profile> profile_;
  base::FilePath profile_path_;
  profiles::ProfileCategoryStats profile_category_stats_;

  // Callback function to be called when results arrive. Will be called
  // multiple times (once for each statistics).
  std::vector<profiles::ProfileStatisticsCallback> stats_callbacks_;

  // Callback function to be called when all statistics are calculated.
  base::OnceClosure done_callback_;

  std::vector<std::unique_ptr<browsing_data::BrowsingDataCounter>> counters_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_
