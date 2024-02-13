// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/local_user_files/policy_utils.h"

#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_features.h"
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
      scoped_feature_list_.InitAndEnableFeature(features::kSkyVault);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kSkyVault);
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
  ASSERT_TRUE(LocalUserFilesAllowed());

  // Set the policy.
  bool allowed = PolicyValue();
  SetPolicyValue(allowed);

  if (!EnableSkyvault()) {
    // Policy value doesn't matter.
    ASSERT_TRUE(LocalUserFilesAllowed());
  } else {
    ASSERT_EQ(LocalUserFilesAllowed(), allowed);
  }
}

INSTANTIATE_TEST_SUITE_P(LocalUserFiles,
                         LocalUserFilesPolicyUtilsBrowserTest,
                         testing::Combine(
                             /*enable_skyvault*/ testing::Bool(),
                             /*policy_value*/ testing::Bool()));
}  // namespace policy::local_user_files
