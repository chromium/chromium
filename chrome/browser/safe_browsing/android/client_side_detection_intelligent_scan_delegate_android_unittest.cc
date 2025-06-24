// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionIntelligentScanDelegateAndroidTest
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroidTest() = default;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldRequestIntelligentScan) {
  ClientSideDetectionIntelligentScanDelegateAndroid delegate;
  ClientPhishingRequest verdict;
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

}  // namespace safe_browsing
