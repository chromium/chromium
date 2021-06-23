// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

// A BrowsingDataCounter that counts the number of downloads as seen on the
// chrome://downloads page.
class DownloadsCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit DownloadsCounter(Profile* profile);
  ~DownloadsCounter() override;

  const char* GetPrefName() const override;

 private:
  // BrowsingDataRemover implementation.
  void Count() override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsCounter);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_DOWNLOADS_COUNTER_H_
