// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionServiceFactoryTest : public testing::Test {
 protected:
  ClientSideDetectionServiceFactoryTest() = default;
  ~ClientSideDetectionServiceFactoryTest() override = default;

  Profile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ClientSideDetectionServiceFactoryTest,
       TestGetForProfileWithCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kDisableClientSidePhishingDetection);
  EXPECT_EQ(nullptr,
            ClientSideDetectionServiceFactory::GetForProfile(profile()));
}

}  // namespace safe_browsing
