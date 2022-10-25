// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/adb_sideloading_availability_delegate.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class FakeAdbSideloadingAvailabilityDelegate
    : public AdbSideloadingAvailabilityDelegate {
 public:
  FakeAdbSideloadingAvailabilityDelegate() = default;
  ~FakeAdbSideloadingAvailabilityDelegate() override = default;

  void set_result(bool result) { result_ = result; }

  void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback)
      override {
    std::move(callback).Run(result_);
  }

 private:
  bool result_ = false;
};

class ArcActivationNecessityCheckerTest : public testing::Test {
 public:
  ArcActivationNecessityCheckerTest() = default;
  ~ArcActivationNecessityCheckerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(kArcOnDemandFeature);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

    checker_ = std::make_unique<ArcActivationNecessityChecker>(
        profile_.get(), &adb_sideloading_availability_delegate_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  FakeAdbSideloadingAvailabilityDelegate adb_sideloading_availability_delegate_;
  std::unique_ptr<ArcActivationNecessityChecker> checker_;
};

TEST_F(ArcActivationNecessityCheckerTest, NotARCVM) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ash::switches::kEnableArcVm);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, FeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kArcOnDemandFeature);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, UnmanagedUser) {
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, AdbSideloadingIsAvailable) {
  adb_sideloading_availability_delegate_.set_result(true);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, NoNeedToActivate) {
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace

}  // namespace arc
