// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataFlashLSOHelper::MockBrowsingDataFlashLSOHelper(
    content::BrowserContext* browser_context) {
}
void MockBrowsingDataFlashLSOHelper::StartFetching(
    GetSitesWithFlashDataCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataFlashLSOHelper::DeleteFlashLSOsForSite(
    const std::string& site,
    base::OnceClosure callback) {
  auto entry = std::find(domains_.begin(), domains_.end(), site);
  ASSERT_TRUE(entry != domains_.end());
  domains_.erase(entry);
  if (!callback.is_null())
    std::move(callback).Run();
}

void MockBrowsingDataFlashLSOHelper::AddFlashLSODomain(
    const std::string& domain) {
  domains_.push_back(domain);
}

void MockBrowsingDataFlashLSOHelper::Notify() {
  std::move(callback_).Run(domains_);
  callback_ = GetSitesWithFlashDataCallback();
}

bool MockBrowsingDataFlashLSOHelper::AllDeleted() {
  return domains_.empty();
}

MockBrowsingDataFlashLSOHelper::~MockBrowsingDataFlashLSOHelper() {
}
