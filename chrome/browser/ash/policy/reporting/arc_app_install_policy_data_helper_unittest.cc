// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_policy_data_helper.h"

#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";

}  // namespace

namespace policy {

class ArcAppInstallPolicyDataHelperTest : public testing::Test {
 public:
  ArcAppInstallPolicyDataHelperTest(const ArcAppInstallPolicyDataHelperTest&) =
      delete;
  ArcAppInstallPolicyDataHelperTest& operator=(
      const ArcAppInstallPolicyDataHelperTest&) = delete;

 protected:
  ArcAppInstallPolicyDataHelperTest() = default;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ArcAppInstallPolicyDataHelper policy_data_helper_;
};

TEST_F(ArcAppInstallPolicyDataHelperTest, AddPolicyData_NoPendingPackages) {
  policy_data_helper_.AddPolicyData({}, 0);
  EXPECT_EQ(0u, policy_data_helper_.policy_data()->size());
  EXPECT_FALSE(policy_data_helper_.policy_data_timer()->IsRunning());
}

TEST_F(ArcAppInstallPolicyDataHelperTest, AddPolicyData) {
  policy_data_helper_.AddPolicyData({kPackageName}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  EXPECT_TRUE(policy_data_helper_.policy_data_timer()->IsRunning());
}

TEST_F(ArcAppInstallPolicyDataHelperTest,
       UpdatePolicySuccessRate_RemoveTrackedPolicy) {
  policy_data_helper_.AddPolicyData({kPackageName}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  policy_data_helper_.UpdatePolicySuccessRate(kPackageName, /* success */ true);
  EXPECT_EQ(0u, policy_data_helper_.policy_data()->size());
}

TEST_F(ArcAppInstallPolicyDataHelperTest,
       UpdatePolicySuccessRate_KeepUntrackedPolicy) {
  policy_data_helper_.AddPolicyData({kPackageName, kPackageName2}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  policy_data_helper_.UpdatePolicySuccessRate(kPackageName, /* success */ true);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
}

TEST_F(ArcAppInstallPolicyDataHelperTest, UpdatePolicySuccessRateForPackages) {
  policy_data_helper_.AddPolicyData({kPackageName}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());

  task_environment.AdvanceClock(base::Minutes(1));
  policy_data_helper_.AddPolicyData({kPackageName, kPackageName2}, 0);
  EXPECT_EQ(2u, policy_data_helper_.policy_data()->size());

  policy_data_helper_.UpdatePolicySuccessRate(kPackageName, /* success */ true);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());

  policy_data_helper_.UpdatePolicySuccessRate(kPackageName2,
                                              /* success */ true);
  EXPECT_EQ(0u, policy_data_helper_.policy_data()->size());
}

TEST_F(ArcAppInstallPolicyDataHelperTest,
       CheckForPolicyDataTimeout_HasTimedout) {
  policy_data_helper_.AddPolicyData({kPackageName}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  EXPECT_TRUE(policy_data_helper_.policy_data_timer()->IsRunning());

  task_environment.AdvanceClock(base::Minutes(30));
  policy_data_helper_.CheckForPolicyDataTimeout();
  EXPECT_EQ(0u, policy_data_helper_.policy_data()->size());
  EXPECT_FALSE(policy_data_helper_.policy_data_timer()->IsRunning());
}

TEST_F(ArcAppInstallPolicyDataHelperTest,
       CheckForPolicyDataTimeout_HasNotTimedout) {
  policy_data_helper_.AddPolicyData({kPackageName}, 0);
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  EXPECT_TRUE(policy_data_helper_.policy_data_timer()->IsRunning());

  policy_data_helper_.CheckForPolicyDataTimeout();
  EXPECT_EQ(1u, policy_data_helper_.policy_data()->size());
  EXPECT_TRUE(policy_data_helper_.policy_data_timer()->IsRunning());
}

}  // namespace policy
