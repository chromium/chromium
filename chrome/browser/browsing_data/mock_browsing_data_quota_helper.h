// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_QUOTA_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_QUOTA_HELPER_H_

#include <stdint.h>

#include <list>
#include <string>

#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"

class MockBrowsingDataQuotaHelper : public BrowsingDataQuotaHelper {
 public:
  MockBrowsingDataQuotaHelper();

  MockBrowsingDataQuotaHelper(const MockBrowsingDataQuotaHelper&) = delete;
  MockBrowsingDataQuotaHelper& operator=(const MockBrowsingDataQuotaHelper&) =
      delete;

  void StartFetching(FetchResultCallback callback) override;
  void DeleteHostData(const std::string& host,
                      blink::mojom::StorageType type) override;

  void AddHost(const std::string& host,
               int64_t temporary_usage,
               int64_t syncable_usage);
  void AddQuotaSamples();
  void Notify();

 private:
  ~MockBrowsingDataQuotaHelper() override;

  FetchResultCallback callback_;
  std::list<QuotaInfo> response_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_QUOTA_HELPER_H_
