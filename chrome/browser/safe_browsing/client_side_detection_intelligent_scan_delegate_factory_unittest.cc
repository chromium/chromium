// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionIntelligentScanDelegateFactoryTest
    : public testing::Test {
 protected:
  ClientSideDetectionIntelligentScanDelegateFactoryTest() = default;
  ~ClientSideDetectionIntelligentScanDelegateFactoryTest() override = default;

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest,
       DisabledIncognito) {
  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  auto* otr_profile = profile()->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                otr_profile));
}

}  // namespace safe_browsing
