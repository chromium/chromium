// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace {

class ChromeBrowserMainExtraPartsNaclDeprecationTest
    : public InProcessBrowserTest {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTest() {
    feature_list_.InitAndDisableFeature(kNaclAllow);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainExtraPartsNaclDeprecationTest,
                       FieldTrialDisable) {
  EXPECT_FALSE(IsNaclAllowed());
}

#if BUILDFLAG(IS_CHROMEOS)
class ChromeBrowserMainExtraPartsNaclDeprecationWithPolicyTest
    : public policy::PolicyTest {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationWithPolicyTest() {
    feature_list_.InitAndDisableFeature(kNaclAllow);
    policy::PolicyMap policies;
    policies.Set(policy::key::kNativeClientForceAllowed,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    UpdateProviderPolicy(policies);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainExtraPartsNaclDeprecationWithPolicyTest,
                       PolicyOverridesFieldTrial) {
  EXPECT_TRUE(IsNaclAllowed());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
