// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionServiceFactoryTest : public testing::Test {
 protected:
  ClientSideDetectionServiceFactoryTest() = default;
  ~ClientSideDetectionServiceFactoryTest() override = default;

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ClientSideDetectionServiceFactoryTest, DisabledIncognito) {
  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  auto* otr_profile = profile()->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            ClientSideDetectionServiceFactory::GetForProfile(otr_profile));
}

}  // namespace safe_browsing
