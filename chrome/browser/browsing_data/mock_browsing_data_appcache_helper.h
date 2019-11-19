// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_

#include <list>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "url/origin.h"

class Profile;

class MockBrowsingDataAppCacheHelper
    : public BrowsingDataAppCacheHelper {
 public:
  explicit MockBrowsingDataAppCacheHelper(Profile* profile);

  void StartFetching(FetchCallback completion_callback) override;
  void DeleteAppCaches(const url::Origin& origin) override;

  // Adds AppCache samples.
  void AddAppCacheSamples();

  // Notifies the callback.
  void Notify();

 private:
  ~MockBrowsingDataAppCacheHelper() override;

  FetchCallback completion_callback_;

  std::list<content::StorageUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockBrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_
