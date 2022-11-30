// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
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
    feature_list_.InitAndEnableFeature(ash::features::kBruschetta);
    service_ = std::make_unique<BruschettaService>(&profile_);
  }

  void TearDown() override {}

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BruschettaService> service_;
};

// GetLauncher returns launcher.
TEST_F(BruschettaServiceTest, GetLauncher) {
  service_->Register(GetBruschettaId());
  ASSERT_NE(service_->GetLauncher("bru"), nullptr);
}

// GetLauncher returns null for unknown vm.
TEST_F(BruschettaServiceTest, GetLauncherNullForUnknown) {
  ASSERT_EQ(service_->GetLauncher("noname"), nullptr);
}

}  // namespace bruschetta
