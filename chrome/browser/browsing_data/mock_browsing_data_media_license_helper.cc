// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_media_license_helper.h"

#include <algorithm>

#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

MockBrowsingDataMediaLicenseHelper::MockBrowsingDataMediaLicenseHelper(
    Profile* profile) {}

MockBrowsingDataMediaLicenseHelper::~MockBrowsingDataMediaLicenseHelper() {}

void MockBrowsingDataMediaLicenseHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataMediaLicenseHelper::DeleteMediaLicenseOrigin(
    const url::Origin& origin) {
  auto entry = std::find_if(media_licenses_.begin(), media_licenses_.end(),
                            [origin](const content::StorageUsageInfo& entry) {
                              return entry.origin == origin;
                            });
  ASSERT_TRUE(entry != media_licenses_.end());
  media_licenses_.erase(entry);
}

void MockBrowsingDataMediaLicenseHelper::AddMediaLicenseSamples() {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://media1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://media2/"));
  const base::Time ten_days_ago = base::Time::Now() - base::Days(10);
  const base::Time twenty_days_ago = base::Time::Now() - base::Days(20);

  media_licenses_.emplace_back(kOrigin1, 1000, ten_days_ago);
  media_licenses_.emplace_back(kOrigin2, 50, twenty_days_ago);
}

void MockBrowsingDataMediaLicenseHelper::Notify() {
  std::move(callback_).Run(media_licenses_);
}

bool MockBrowsingDataMediaLicenseHelper::AllDeleted() {
  return media_licenses_.empty();
}
