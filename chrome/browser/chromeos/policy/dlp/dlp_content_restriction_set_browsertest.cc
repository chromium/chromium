// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#endif

namespace policy {

// TODO(crbug.com/1262948): Enable and modify for lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)

const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kUrl1[] = "https://example1.com";
constexpr char kUrl2[] = "https://example2.com";
constexpr char kUrl3[] = "https://example3.com";
constexpr char kUrl4[] = "https://example4.com";

class DlpContentRestrictionSetBrowserTest : public LoginPolicyTestBase {
 public:
  DlpContentRestrictionSetBrowserTest() = default;

  void SetDlpRulesPolicy(const base::Value& rules) {
    std::string json;
    base::JSONWriter::Write(rules, &json);

    enterprise_management::CloudPolicySettings policy;
    policy.mutable_dataleakpreventionruleslist()->set_value(json);
    user_policy_helper()->SetPolicyAndWait(
        policy, ProfileManager::GetActiveUserProfile());
  }
};

IN_PROC_BROWSER_TEST_F(DlpContentRestrictionSetBrowserTest,
                       GetRestrictionSetForURL) {
  SkipToLoginScreen();
  LogIn();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kUrl1);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kUrl2);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrivacyScreenRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Block", std::move(src_urls2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  base::Value src_urls3(base::Value::Type::LIST);
  src_urls3.Append(kUrl3);
  base::Value restrictions3(base::Value::Type::LIST);
  restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrintingRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #3", "Block", std::move(src_urls3),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions3)));

  base::Value src_urls4(base::Value::Type::LIST);
  src_urls4.Append(kUrl4);
  base::Value restrictions4(base::Value::Type::LIST);
  restrictions4.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #4", "Block", std::move(src_urls4),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions4)));

  SetDlpRulesPolicy(rules);

  EXPECT_EQ(kScreenshotRestricted,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl1)));
  EXPECT_EQ(kPrivacyScreenEnforced,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl2)));
  EXPECT_EQ(kPrintRestricted, DlpContentRestrictionSet::GetForURL(GURL(kUrl3)));
  EXPECT_EQ(kScreenShareRestricted,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl4)));
  EXPECT_EQ(DlpContentRestrictionSet(),
            DlpContentRestrictionSet::GetForURL(GURL(kExampleUrl)));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
