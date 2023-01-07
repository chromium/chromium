// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"

class Profile;

class SiteDataCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit SiteDataCounter(Profile* profile);
  ~SiteDataCounter() override;

  const char* GetPrefName() const override;

 private:
  void OnInitialized() override;
  void Count() override;
  void Done(int origin_count);

  raw_ptr<Profile> profile_;
  browsing_data::SyncTracker sync_tracker_;
  base::WeakPtrFactory<SiteDataCounter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTER_H_
