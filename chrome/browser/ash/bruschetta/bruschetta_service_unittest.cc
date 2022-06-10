// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

class BruschettaServiceTest : public testing::Test {
 public:
  BruschettaServiceTest() = default;
  BruschettaServiceTest(const BruschettaServiceTest&) = delete;
  BruschettaServiceTest& operator=(const BruschettaServiceTest&) = delete;
  ~BruschettaServiceTest() override = default;

 protected:
  void SetUp() override {
    service_ = std::make_unique<BruschettaService>(&profile_);
  }

  void TearDown() override {}

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BruschettaService> service_;
};

// GetLauncher returns launcher.
TEST_F(BruschettaServiceTest, GetLauncher) {
  ASSERT_NE(service_->GetLauncher("bru"), nullptr);
}

// GetLauncher returns null for unknown vm.
TEST_F(BruschettaServiceTest, GetLauncherNullForUnknown) {
  ASSERT_EQ(service_->GetLauncher("noname"), nullptr);
}

}  // namespace bruschetta
