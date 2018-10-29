// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_quota_helper.h"

#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

MockBrowsingDataQuotaHelper::MockBrowsingDataQuotaHelper(Profile* profile)
    : BrowsingDataQuotaHelper() {}

MockBrowsingDataQuotaHelper::~MockBrowsingDataQuotaHelper() {}

void MockBrowsingDataQuotaHelper::StartFetching(FetchResultCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataQuotaHelper::RevokeHostQuota(const std::string& host) {
}

void MockBrowsingDataQuotaHelper::AddHost(const std::string& host,
                                          int64_t temporary_usage,
                                          int64_t persistent_usage,
                                          int64_t syncable_usage) {
  response_.push_back(QuotaInfo(
      host,
      temporary_usage,
      persistent_usage,
      syncable_usage));
}

void MockBrowsingDataQuotaHelper::AddQuotaSamples() {
  AddHost("quotahost1", 1, 2, 1);
  AddHost("quotahost2", 10, 20, 10);
}

void MockBrowsingDataQuotaHelper::Notify() {
  std::move(callback_).Run(response_);
  response_.clear();
}
