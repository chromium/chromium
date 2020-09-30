// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {
constexpr char kUrlStr1[] = "https://wwww.example.com";
}

class DlpRulesPolicyTest : public PolicyTest {
 public:
  DlpRulesPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDataLeakPreventionPolicy);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, ParsePolicyPref) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrlStr1);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));

  PolicyMap policies;
  policies.Set(key::kDataLeakPreventionRulesList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(rules),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            DlpRulesManager::Get()->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
}

}  // namespace policy
