// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kUrlStr1[] = "https://wwww.example.com";
constexpr char kUrlStr2[] = "https://wwww.google.com";
constexpr char kUrlStr3[] = "*";
constexpr char kUrlStr4[] = "https://www.gmail.com";

constexpr char kHttpsPrefix[] = "https://www.";

constexpr char kUrlPattern1[] = "chat.google.com";
constexpr char kUrlPattern2[] = "salesforce.com";
constexpr char kUrlPattern3[] = "docs.google.com";
constexpr char kUrlPattern4[] = "drive.google.com";
constexpr char kUrlPattern5[] = "*.company.com";

base::Value GenerateClipboardCopyDisallowedRule() {
  base::Value rules(base::Value::Type::LIST);
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrlStr1);
  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kUrlStr3);
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));
  return rules;
}

}  // namespace

class DlpRulesManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kDataLeakPreventionPolicy);

    DlpRulesManager::Init();
    dlp_rules_manager_ = DlpRulesManager::Get();
  }

  DlpRulesManagerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void UpdatePolicyPref(base::Value rules_list) {
    DCHECK(rules_list.is_list());
    testing_local_state_.Get()->Set(policy_prefs::kDlpRulesList,
                                    std::move(rules_list));
    // TODO(crbug.com/1131392): Remove OnPolicyUpdate call.
    dlp_rules_manager_->OnPolicyUpdate();
  }

  DlpRulesManager* dlp_rules_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  ScopedTestingLocalState testing_local_state_;
};

TEST_F(DlpRulesManagerTest, EmptyPref) {
  UpdatePolicyPref(base::Value(base::Value::Type::LIST));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kPrinting));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr2),
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, IsRestricted_LevelPrecedence) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kUrlStr1);

  base::Value dst_urls_1(base::Value::Type::LIST);
  dst_urls_1.Append(kUrlStr3);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls_1), std::move(dst_urls_1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  // Second Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kUrlStr1);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kUrlStr2);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "exceptional allow", std::move(src_urls_2),
      std::move(dst_urls_2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));
  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr2),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr4),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));

  // Clear pref
  UpdatePolicyPref(base::Value(base::Value::Type::LIST));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr2),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr4),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerTest, UpdatePref) {
  // First DLP rule
  base::Value rules_1(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kUrlStr1);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));

  rules_1.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls_1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));
  UpdatePolicyPref(std::move(rules_1));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));

  // Second DLP rule
  base::Value rules_2(base::Value::Type::LIST);

  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kUrlStr2);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));

  rules_2.Append(dlp_test_util::CreateRule(
      "rule #2", "exceptional allow", std::move(src_urls_2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));
  UpdatePolicyPref(std::move(rules_2));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr2), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerTest, IsRestrictedComponent_Clipboard) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrlStr1);

  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append("ARC");

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));
  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedComponent(
                GURL(kUrlStr1), DlpRulesManager::Component::kArc,
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedComponent(
                GURL(kUrlStr1), DlpRulesManager::Component::kCrostini,
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, SameSrcDst_Clipboard) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr1),
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, EmptyUrl_Clipboard) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  // Second Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kUrlStr4);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kUrlStr2);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Block", std::move(src_urls_2), std::move(dst_urls_2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(
      DlpRulesManager::Level::kBlock,
      dlp_rules_manager_->IsRestrictedDestination(
          GURL(kUrlStr1), GURL(), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(
      DlpRulesManager::Level::kAllow,
      dlp_rules_manager_->IsRestrictedDestination(
          GURL(kUrlStr4), GURL(), DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, IsRestrictedAnyOfComponents_Clipboard) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrlStr1);

  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append(dlp::kPluginVm);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block PluginVM", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedAnyOfComponents(
                GURL(kUrlStr1),
                std::vector<DlpRulesManager::Component>{
                    DlpRulesManager::Component::kPluginVm,
                    DlpRulesManager::Component::kCrostini},
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedAnyOfComponents(
                GURL(kUrlStr1),
                std::vector<DlpRulesManager::Component>{
                    DlpRulesManager::Component::kArc,
                    DlpRulesManager::Component::kCrostini},
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, IsRestricted_MultipleURLs) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kUrlPattern1);
  src_urls_1.Append(kUrlPattern2);
  src_urls_1.Append(kUrlPattern3);
  src_urls_1.Append(kUrlPattern4);
  src_urls_1.Append(kUrlPattern5);

  base::Value dst_urls_1 = src_urls_1.Clone();
  base::Value src_urls_2 = src_urls_1.Clone();

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Support agent work flows", "Allow copy and paste for work purposes",
      std::move(src_urls_1), std::move(dst_urls_1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kUrlStr3);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Block non-agent work flows",
      "Disallow copy and paste for non-work purposes", std::move(src_urls_2),
      std::move(dst_urls_2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern1})),
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern2})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern3})),
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern4})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern5})),
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern2})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern2})),
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern3})),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern1})),
                GURL(kUrlStr2), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern2})),
                GURL(kUrlStr1), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern3})),
                GURL(kUrlStr2), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kUrlPattern4})),
                GURL(kUrlStr1), DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, DisabledByFeature) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr3),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));

  // Disable feature
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      features::kDataLeakPreventionPolicy);
  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr3),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
}

}  // namespace policy
