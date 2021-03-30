// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://wwww.example.com";
constexpr char kGoogleUrl[] = "https://wwww.google.com";
constexpr char kWildCardMatching[] = "*";
constexpr char kGmailUrl[] = "https://www.gmail.com";

constexpr char kHttpsPrefix[] = "https://www.";

constexpr char kChatPattern[] = "chat.google.com";
constexpr char kSalesforcePattern[] = "salesforce.com";
constexpr char kDocsPattern[] = "docs.google.com";
constexpr char kDrivePattern[] = "drive.google.com";
constexpr char kCompanyPattern[] = "*.company.com";
constexpr char kGooglePatten[] = "google.com";
constexpr char kMailPattern[] = "mail.google.com";

base::Value GenerateClipboardCopyDisallowedRule() {
  base::Value rules(base::Value::Type::LIST);
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);
  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kWildCardMatching);
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

class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
};

class DlpRulesManagerImplTest : public testing::Test {
 protected:
  DlpRulesManagerImplTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        dlp_rules_manager_(testing_local_state_.Get()) {}

  void UpdatePolicyPref(base::Value rules_list) {
    DCHECK(rules_list.is_list());
    testing_local_state_.Get()->Set(policy_prefs::kDlpRulesList,
                                    std::move(rules_list));
  }

  ScopedTestingLocalState testing_local_state_;
  MockDlpRulesManager dlp_rules_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DlpRulesManagerImplTest, EmptyPref) {
  UpdatePolicyPref(base::Value(base::Value::Type::LIST));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kPrinting));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard));
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDlpPolicyPresentUMA, false, 1);
}

TEST_F(DlpRulesManagerImplTest, BlockPriority) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kExampleUrl);

  base::Value dst_urls_1(base::Value::Type::LIST);
  dst_urls_1.Append(kWildCardMatching);

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
  src_urls_2.Append(kExampleUrl);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kGoogleUrl);

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
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGmailUrl),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDlpPolicyPresentUMA, true, 1);
  histogram_tester_.ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                      DlpRulesManager::Restriction::kClipboard,
                                      2);
  histogram_tester_.ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                      DlpRulesManager::Restriction::kScreenshot,
                                      1);

  // Clear pref
  UpdatePolicyPref(base::Value(base::Value::Type::LIST));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGmailUrl),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerImplTest, UpdatePref) {
  // First DLP rule
  base::Value rules_1(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kExampleUrl);

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
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  // Second DLP rule
  base::Value rules_2(base::Value::Type::LIST);

  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kGoogleUrl);

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
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerImplTest, IsRestrictedComponent_Clipboard) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);

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
            dlp_rules_manager_.IsRestrictedComponent(
                GURL(kExampleUrl), DlpRulesManager::Component::kArc,
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedComponent(
                GURL(kExampleUrl), DlpRulesManager::Component::kCrostini,
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerImplTest, SameSrcDst_Clipboard) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kExampleUrl),
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerImplTest, EmptyUrl_Clipboard) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  // Second Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kGmailUrl);

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kGoogleUrl);

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
      dlp_rules_manager_.IsRestrictedDestination(
          GURL(kExampleUrl), GURL(), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(
      DlpRulesManager::Level::kAllow,
      dlp_rules_manager_.IsRestrictedDestination(
          GURL(kGmailUrl), GURL(), DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerImplTest, IsRestricted_MultipleURLs) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kChatPattern);
  src_urls_1.Append(kSalesforcePattern);
  src_urls_1.Append(kDocsPattern);
  src_urls_1.Append(kDrivePattern);
  src_urls_1.Append(kCompanyPattern);

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
  dst_urls_2.Append(kWildCardMatching);

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
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kCompanyPattern})),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                DlpRulesManager::Restriction::kClipboard));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerImplTest, DisabledByFeature) {
  base::Value rules = GenerateClipboardCopyDisallowedRule();

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kWildCardMatching),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  // Disable feature
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDataLeakPreventionPolicy);
  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kWildCardMatching),
                DlpRulesManager::Restriction::kClipboard));
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerImplTest, WarnPriority) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kGooglePatten);

  base::Value dst_urls_1(base::Value::Type::LIST);
  dst_urls_1.Append(kWildCardMatching);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kWarnLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Warn on every copy from google.com", std::move(src_urls_1),
      std::move(dst_urls_1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  // Second Rule
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kDocsPattern);
  src_urls_2.Append(kDrivePattern);
  src_urls_2.Append(kMailPattern);
  base::Value src_urls_3 = src_urls_2.Clone();

  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kWildCardMatching);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Block copy/paste from docs, drive, gmail",
      std::move(src_urls_2), std::move(dst_urls_2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  // Third Rule
  base::Value dst_urls_3 = src_urls_3.Clone();

  base::Value restrictions_3(base::Value::Type::LIST);
  restrictions_3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #3", "Allow copy/paste inside docs, drive, gmail",
      std::move(src_urls_3), std::move(dst_urls_3),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_3)));

  UpdatePolicyPref(std::move(rules));

  // Copy/paste from chat.google to example.com should be warned.
  EXPECT_EQ(DlpRulesManager::Level::kWarn,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard));

  // Copy/paste from docs to salesforce should be blocked.
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard));

  // Copy/paste from docs to gmail should be allowed.
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kMailPattern})),
                DlpRulesManager::Restriction::kClipboard));
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_DlpClientNotified) {
  content::BrowserTaskEnvironment task_environment;
  chromeos::DlpClient::InitializeFake();

  EXPECT_EQ(0, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);

  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kExampleUrl);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Files", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));
  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(1, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());
}

}  // namespace policy
