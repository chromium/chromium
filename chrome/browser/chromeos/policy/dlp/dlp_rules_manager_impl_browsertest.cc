// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {
constexpr char kUrlStr1[] = "https://wwww.example.com";
}

class DlpRulesPolicyTest : public LoginPolicyTestBase {
 public:
  DlpRulesPolicyTest() = default;

  void SetDlpRulesPolicy(const base::Value& rules) {
    std::string json;
    base::JSONWriter::Write(rules, &json);

    base::DictionaryValue policy;
    policy.SetKey(key::kDataLeakPreventionRulesList, base::Value(json));
    user_policy_helper()->SetPolicyAndWait(
        policy, /*recommended=*/base::DictionaryValue(),
        ProfileManager::GetActiveUserProfile());
  }
};

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, ParsePolicyPref) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

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

  SetDlpRulesPolicy(rules);

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            DlpRulesManagerFactory::GetForPrimaryProfile()->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
}

}  // namespace policy
