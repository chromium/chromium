// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_TABS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_TABS_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

// A BrowsingDataCounter that counts the number of open tabs.
class TabsCounter : public browsing_data::BrowsingDataCounter {
 public:
  class TabsResult : public FinishedResult {
   public:
    TabsResult(const TabsCounter* source,
               ResultInt tab_count,
               ResultInt window_count);

    TabsResult(const TabsResult&) = delete;
    TabsResult& operator=(const TabsResult&) = delete;

    ~TabsResult() override;

    int window_count() const { return window_count_; }

   private:
    int window_count_;
  };

  explicit TabsCounter(Profile* profile);

  TabsCounter(const TabsCounter&) = delete;
  TabsCounter& operator=(const TabsCounter&) = delete;

  ~TabsCounter() override;

  const char* GetPrefName() const override;

 private:
  void Count() override;

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_TABS_COUNTER_H_
