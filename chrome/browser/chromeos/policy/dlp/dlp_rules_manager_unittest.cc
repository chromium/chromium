// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <string>

#include "base/values.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kName[] = "name";
constexpr char kDescription[] = "description";
constexpr char kSources[] = "sources";
constexpr char kUrls[] = "urls";
constexpr char kDestinations[] = "destinations";
constexpr char kComponents[] = "components";
constexpr char kRestrictions[] = "restrictions";
constexpr char kClass[] = "class";
constexpr char kLevel[] = "level";

constexpr char kUrlStr1[] = "https://wwww.example.com";
constexpr char kUrlStr2[] = "https://wwww.google.com";
constexpr char kUrlStr3[] = "*";
constexpr char kUrlStr4[] = "https://www.gmail.com";

base::Value CreateSources(base::Value urls) {
  base::Value srcs(base::Value::Type::DICTIONARY);
  srcs.SetKey(kUrls, std::move(urls));
  return srcs;
}

base::Value CreateDestinations(base::Value urls, base::Value components) {
  base::Value dsts(base::Value::Type::DICTIONARY);
  dsts.SetKey(kUrls, std::move(urls));
  dsts.SetKey(kComponents, std::move(components));
  return dsts;
}

base::Value CreateRestrictionWithLevel(const std::string& restriction,
                                       const std::string& level) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kClass, restriction);
  dict.SetStringKey(kLevel, level);
  return dict;
}

base::Value CreateRule(const std::string& name,
                       const std::string& desc,
                       base::Value src_urls,
                       base::Value dst_urls,
                       base::Value dst_components,
                       base::Value restrictions) {
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetStringKey(kName, name);
  rule.SetStringKey(kDescription, desc);
  DCHECK(src_urls.is_list());
  rule.SetKey(kSources, CreateSources(std::move(src_urls)));
  DCHECK(dst_urls.is_list());
  DCHECK(dst_components.is_list());
  rule.SetKey(kDestinations, CreateDestinations(std::move(dst_urls),
                                                std::move(dst_components)));
  DCHECK(restrictions.is_list());
  rule.SetKey(kRestrictions, std::move(restrictions));
  return rule;
}

}  // namespace

class DlpRulesManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();

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
  restrictions_1.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kBlockLevel));
  restrictions_1.Append(CreateRestrictionWithLevel(dlp::kScreenshotRestriction,
                                                   dlp::kBlockLevel));

  rules.Append(CreateRule(
      "rule #1", "Block", std::move(src_urls_1), std::move(dst_urls_1),
      base::Value(base::Value::Type::LIST), std::move(restrictions_1)));

  // Second Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kUrlStr1);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kUrlStr2);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kAllowLevel));

  rules.Append(CreateRule("rule #2", "exceptional allow", std::move(src_urls_2),
                          std::move(dst_urls_2),
                          base::Value(base::Value::Type::LIST),
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
  restrictions_1.Append(CreateRestrictionWithLevel(dlp::kScreenshotRestriction,
                                                   dlp::kBlockLevel));

  rules_1.Append(CreateRule("rule #1", "Block", std::move(src_urls_1),
                            base::Value(base::Value::Type::LIST),
                            base::Value(base::Value::Type::LIST),
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
  restrictions_2.Append(CreateRestrictionWithLevel(dlp::kScreenshotRestriction,
                                                   dlp::kBlockLevel));

  rules_2.Append(CreateRule(
      "rule #2", "exceptional allow", std::move(src_urls_2),
      base::Value(base::Value::Type::LIST),
      base::Value(base::Value::Type::LIST), std::move(restrictions_2)));
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
  restrictions.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(CreateRule("rule #1", "Block", std::move(src_urls),
                          base::Value(base::Value::Type::LIST),
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
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrlStr1);

  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kUrlStr3);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(CreateRule(
      "rule #1", "Block", std::move(src_urls), std::move(dst_urls),
      base::Value(base::Value::Type::LIST), std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestrictedDestination(
                GURL(kUrlStr1), GURL(kUrlStr1),
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerTest, EmptyUrl_Clipboard) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kUrlStr1);

  base::Value dst_urls_1(base::Value::Type::LIST);
  dst_urls_1.Append(kUrlStr3);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(CreateRule(
      "rule #1", "Block *", std::move(src_urls_1), std::move(dst_urls_1),
      base::Value(base::Value::Type::LIST), std::move(restrictions_1)));

  // First Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kUrlStr4);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kUrlStr2);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(
      CreateRestrictionWithLevel(dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(CreateRule(
      "rule #2", "Block", std::move(src_urls_2), std::move(dst_urls_2),
      base::Value(base::Value::Type::LIST), std::move(restrictions_2)));

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

}  // namespace policy
