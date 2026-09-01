// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

#include <tuple>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

class LocalUserFilesPolicyUtilsBrowserTest
    : public policy::PolicyTest,
      public ::testing::WithParamInterface<
          std::tuple</*enable_skyvault*/ bool, /*policy_value*/ bool>> {
 public:
  static std::string ParamToString(
      const testing::TestParamInfo<ParamType> info) {
    auto [enable_skyvault, policy_value] = info.param;
    std::string name =
        (enable_skyvault ? "SkyVaultEnabled" : "SkyVaultDisabled");
    name += (policy_value ? "PolicyTrue" : "PolicyFalse");
    return name;
  }

  LocalUserFilesPolicyUtilsBrowserTest() {
    if (EnableSkyvault()) {
      scoped_feature_list_.InitAndEnableFeature(ash::features::kSkyVault);
    } else {
      scoped_feature_list_.InitAndDisableFeature(ash::features::kSkyVault);
    }
  }
  ~LocalUserFilesPolicyUtilsBrowserTest() override = default;

 protected:
  bool EnableSkyvault() { return std::get<0>(GetParam()); }
  bool PolicyValue() { return std::get<1>(GetParam()); }

  void SetPolicyValue(bool local_user_files_allowed) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kLocalUserFilesAllowed,
                                  base::Value(local_user_files_allowed));
    provider_.UpdateChromePolicy(policies);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(LocalUserFilesPolicyUtilsBrowserTest, CheckPolicyValue) {
  // Before setting the policy, local files should always be allowed.
  ASSERT_TRUE(
      LocalUserFilesAllowed(CHECK_DEREF(g_browser_process->local_state())));

  // Set the policy.
  bool allowed = PolicyValue();
  SetPolicyValue(allowed);

  if (!EnableSkyvault()) {
    // Policy value doesn't matter.
    ASSERT_TRUE(
        LocalUserFilesAllowed(CHECK_DEREF(g_browser_process->local_state())));
  } else {
    ASSERT_EQ(
        LocalUserFilesAllowed(CHECK_DEREF(g_browser_process->local_state())),
        allowed);
  }
}

INSTANTIATE_TEST_SUITE_P(LocalUserFiles,
                         LocalUserFilesPolicyUtilsBrowserTest,
                         testing::Combine(
                             /*enable_skyvault*/ testing::Bool(),
                             /*policy_value*/ testing::Bool()),
                         LocalUserFilesPolicyUtilsBrowserTest::ParamToString);

class DownloadsDestinationUtilsTest : public policy::PolicyTest {
 protected:
  void SetDownloadsPolicy(const std::string& destination) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies, policy::key::kDownloadDirectory,
                                  base::Value(destination));
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(DownloadsDestinationUtilsTest, DownloadsDestination) {
  EXPECT_EQ(FileSaveDestination::kNotSpecified,
            GetDownloadsDestination(browser()->GetProfile()));

  SetDownloadsPolicy("");
  EXPECT_EQ(FileSaveDestination::kDownloads,
            GetDownloadsDestination(browser()->GetProfile()));

  SetDownloadsPolicy(kGoogleDrivePolicyVariableName);
  EXPECT_EQ(FileSaveDestination::kGoogleDrive,
            GetDownloadsDestination(browser()->GetProfile()));

  SetDownloadsPolicy(kOneDrivePolicyVariableName);
  EXPECT_EQ(FileSaveDestination::kOneDrive,
            GetDownloadsDestination(browser()->GetProfile()));
}

class ScreenCaptureDestinationUtilsTest : public policy::PolicyTest {
 protected:
  void SetScreenCapturePolicy(const std::string& destination) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kScreenCaptureLocation,
                                  base::Value(destination));
    provider_.UpdateChromePolicy(policies);
  }
  base::test::ScopedFeatureList scoped_feature_list_{ash::features::kSkyVault};
};

IN_PROC_BROWSER_TEST_F(ScreenCaptureDestinationUtilsTest,
                       ScreenCaptureDestination) {
  EXPECT_EQ(FileSaveDestination::kNotSpecified,
            GetScreenCaptureDestination(browser()->GetProfile()));

  SetScreenCapturePolicy("");
  EXPECT_EQ(FileSaveDestination::kDownloads,
            GetScreenCaptureDestination(browser()->GetProfile()));

  SetScreenCapturePolicy(kGoogleDrivePolicyVariableName);
  EXPECT_EQ(FileSaveDestination::kGoogleDrive,
            GetScreenCaptureDestination(browser()->GetProfile()));

  SetScreenCapturePolicy(kOneDrivePolicyVariableName);
  EXPECT_EQ(FileSaveDestination::kOneDrive,
            GetScreenCaptureDestination(browser()->GetProfile()));
}

class DownloadsDestinationUtilsTestWithSkyvault
    : public DownloadsDestinationUtilsTest {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{ash::features::kSkyVault};
};

IN_PROC_BROWSER_TEST_F(DownloadsDestinationUtilsTestWithSkyvault,
                       DownloadToTemp) {
  EXPECT_EQ(false, DownloadToTemp(browser()->GetProfile()));

  SetDownloadsPolicy(kGoogleDrivePolicyVariableName);
  EXPECT_EQ(false, DownloadToTemp(browser()->GetProfile()));

  SetDownloadsPolicy(kOneDrivePolicyVariableName);
  EXPECT_EQ(true, DownloadToTemp(browser()->GetProfile()));
}

}  // namespace policy::local_user_files
