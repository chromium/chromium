// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_policy_data.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";

}  // namespace

namespace policy {

class ArcAppInstallPolicyDataTest : public testing::Test {
 public:
  ArcAppInstallPolicyDataTest(const ArcAppInstallPolicyDataTest&) = delete;
  ArcAppInstallPolicyDataTest& operator=(const ArcAppInstallPolicyDataTest&) =
      delete;

 protected:
  ArcAppInstallPolicyDataTest() = default;
  base::HistogramTester tester;
};

TEST_F(ArcAppInstallPolicyDataTest, InvalidConstruction) {
  EXPECT_DCHECK_DEATH({
    ArcAppInstallPolicyData policy_data_ =
        ArcAppInstallPolicyData(base::TimeTicks::Now(), {}, 0);
  });
}

TEST_F(ArcAppInstallPolicyDataTest, UpdatePackageInstallResult_HundredPercent) {
  ArcAppInstallPolicyData policy_data_(base::TimeTicks::Now(),
                                       {kPackageName, kPackageName2}, 0);

  bool result;
  result = policy_data_.UpdatePackageInstallResult(kPackageName, true);
  EXPECT_FALSE(result);

  result = policy_data_.UpdatePackageInstallResult(kPackageName2, true);
  EXPECT_TRUE(result);

  tester.ExpectUniqueSample("Arc.AppInstall.PolicySuccessRate", 100, 1);
}

TEST_F(ArcAppInstallPolicyDataTest, UpdatePackageInstallResult_FiftyPercent) {
  ArcAppInstallPolicyData policy_data_(base::TimeTicks::Now(),
                                       {kPackageName, kPackageName2}, 0);

  bool result;
  result = policy_data_.UpdatePackageInstallResult(kPackageName, true);
  EXPECT_FALSE(result);

  result = policy_data_.UpdatePackageInstallResult(kPackageName2, false);
  EXPECT_TRUE(result);

  tester.ExpectUniqueSample("Arc.AppInstall.PolicySuccessRate", 50, 1);
}

TEST_F(ArcAppInstallPolicyDataTest, UpdatePackageInstallResult_Timeout) {
  ArcAppInstallPolicyData policy_data_(
      base::TimeTicks::Now() - base::Minutes(45), {kPackageName}, 1);

  bool result = policy_data_.UpdatePackageInstallResult("not_pending", false);
  EXPECT_TRUE(result);

  tester.ExpectUniqueSample("Arc.AppInstall.PolicySuccessRate", 50, 1);
}

TEST_F(ArcAppInstallPolicyDataTest, MaybeRecordSuccessRate_Timeout) {
  ArcAppInstallPolicyData policy_data_(
      base::TimeTicks::Now() - base::Minutes(45), {kPackageName, kPackageName2},
      0);

  bool result = policy_data_.MaybeRecordSuccessRate();
  EXPECT_TRUE(result);
  tester.ExpectUniqueSample("Arc.AppInstall.PolicySuccessRate", 0, 1);
}

TEST_F(ArcAppInstallPolicyDataTest, MaybeRecordSuccessRate_NoTimeout) {
  ArcAppInstallPolicyData policy_data_(base::TimeTicks::Now(),
                                       {kPackageName, kPackageName2}, 0);

  bool result = policy_data_.MaybeRecordSuccessRate();
  EXPECT_FALSE(result);
  tester.ExpectUniqueSample("Arc.AppInstall.PolicySuccessRate", 0, 0);
}

}  // namespace policy
