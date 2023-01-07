// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
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

constexpr char kExampleUrl[] = "https://www.example.com";
constexpr char kGoogleUrl[] = "https://www.google.com";
constexpr char kWildCardMatching[] = "*";
constexpr char kGmailUrl[] = "https://www.gmail.com";
constexpr char kCompanyUrl[] = "https://company.com";

constexpr char kHttpsPrefix[] = "https://www.";

constexpr char kChatPattern[] = "chat.google.com";
constexpr char kSalesforcePattern[] = "salesforce.com";
constexpr char kDocsPattern[] = "docs.google.com";
constexpr char kDrivePattern[] = "drive.google.com";
constexpr char kCompanyPattern[] = ".company.com";
constexpr char kGooglePattern[] = "google.com";
constexpr char kMailPattern[] = "mail.google.com";

class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
};

}  // namespace

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

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  MockDlpRulesManager dlp_rules_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DlpRulesManagerImplTest, EmptyPref) {
  UpdatePolicyPref(base::Value(base::Value::Type::LIST));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kPrinting));
  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDlpPolicyPresentUMA, false, 1);
}

TEST_F(DlpRulesManagerImplTest, UnknownRestriction) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);

  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kWildCardMatching);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      "Wrong restriction", dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Unknown", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));
  histogram_tester_.ExpectBucketCount(
      "Enterprise.Dlp.RestrictionConfigured",
      DlpRulesManager::Restriction::kUnknownRestriction, 0);
}

TEST_F(DlpRulesManagerImplTest, UnknownComponent) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);

  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append("Wrong component");

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Unknown", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));
  histogram_tester_.ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                      DlpRulesManager::Restriction::kClipboard,
                                      1);

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(
      DlpRulesManager::Level::kBlock,
      dlp_rules_manager_.IsRestrictedComponent(
          GURL(kExampleUrl), DlpRulesManager::Component::kUnknownComponent,
          DlpRulesManager::Restriction::kClipboard, &src_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
}

TEST_F(DlpRulesManagerImplTest, UnknownLevel) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);

  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kWildCardMatching);

  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, "Wrong level"));

  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Unknown", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));
  histogram_tester_.ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                      DlpRulesManager::Restriction::kClipboard,
                                      0);
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

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
  EXPECT_EQ(dst_pattern, kGoogleUrl);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGmailUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  src_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedByAnyRule(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
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

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGoogleUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
  EXPECT_EQ(dst_pattern, std::string(""));

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kGmailUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
  EXPECT_EQ(dst_pattern, std::string(""));

  src_pattern.clear();
  dst_pattern.clear();
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

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedComponent(
                GURL(kExampleUrl), DlpRulesManager::Component::kArc,
                DlpRulesManager::Restriction::kClipboard, &src_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);

  src_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedComponent(
                GURL(kExampleUrl), DlpRulesManager::Component::kCrostini,
                DlpRulesManager::Restriction::kClipboard, &src_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
}

TEST_F(DlpRulesManagerImplTest, SameSrcDst_Clipboard) {
  base::Value rules(base::Value::Type::LIST);
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);
  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kWildCardMatching);
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kExampleUrl),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
  EXPECT_EQ(dst_pattern, std::string(""));
}

TEST_F(DlpRulesManagerImplTest, EmptyUrl_Clipboard) {
  base::Value rules(base::Value::Type::LIST);
  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);
  base::Value dst_urls(base::Value::Type::LIST);
  dst_urls.Append(kWildCardMatching);
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls), std::move(dst_urls),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions)));

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

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(
      DlpRulesManager::Level::kBlock,
      dlp_rules_manager_.IsRestrictedDestination(
          GURL(kExampleUrl), GURL(), DlpRulesManager::Restriction::kClipboard,
          &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(
      DlpRulesManager::Level::kAllow,
      dlp_rules_manager_.IsRestrictedDestination(
          GURL(kGmailUrl), GURL(), DlpRulesManager::Restriction::kClipboard,
          &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
  EXPECT_EQ(dst_pattern, std::string(""));
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

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kChatPattern);
  EXPECT_EQ(dst_pattern, kSalesforcePattern);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kDocsPattern);
  EXPECT_EQ(dst_pattern, kDrivePattern);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kCompanyUrl),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kCompanyPattern);
  EXPECT_EQ(dst_pattern, kSalesforcePattern);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kSalesforcePattern);
  EXPECT_EQ(dst_pattern, kDocsPattern);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kChatPattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kSalesforcePattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kDocsPattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kDrivePattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);
}

TEST_F(DlpRulesManagerImplTest, DisabledByFeature) {
  base::Value rules_1(base::Value::Type::LIST);
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kExampleUrl);
  base::Value dst_urls_1(base::Value::Type::LIST);
  dst_urls_1.Append(kWildCardMatching);
  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));
  rules_1.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls_1), std::move(dst_urls_1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  UpdatePolicyPref(std::move(rules_1));

  std::string src_pattern;
  std::string dst_pattern;
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kWildCardMatching),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kExampleUrl);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  // Disable feature
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDataLeakPreventionPolicy);

  base::Value rules_2(base::Value::Type::LIST);
  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kExampleUrl);
  base::Value dst_urls_2(base::Value::Type::LIST);
  dst_urls_2.Append(kWildCardMatching);
  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules_2.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls_2), std::move(dst_urls_2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  UpdatePolicyPref(std::move(rules_2));

  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(kExampleUrl), GURL(kWildCardMatching),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, std::string(""));
  EXPECT_EQ(dst_pattern, std::string(""));
}

TEST_F(DlpRulesManagerImplTest, WarnPriority) {
  base::Value rules(base::Value::Type::LIST);

  // First Rule
  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kGooglePattern);

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

  std::string src_pattern;
  std::string dst_pattern;

  // Copy/paste from chat.google to example.com should be warned.
  EXPECT_EQ(DlpRulesManager::Level::kWarn,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard,
                &src_pattern, &dst_pattern));
  EXPECT_EQ(src_pattern, kGooglePattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  // Copy/paste from docs to salesforce should be blocked.
  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kDocsPattern);
  EXPECT_EQ(dst_pattern, kWildCardMatching);

  // Copy/paste from docs to gmail should be allowed.
  src_pattern.clear();
  dst_pattern.clear();
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestrictedDestination(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                GURL(base::StrCat({kHttpsPrefix, kMailPattern})),
                DlpRulesManager::Restriction::kClipboard, &src_pattern,
                &dst_pattern));
  EXPECT_EQ(src_pattern, kDocsPattern);
  EXPECT_EQ(dst_pattern, kMailPattern);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DlpRulesManagerImplTest, FilesRestriction_DlpClientNotified) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
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
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(dlp_rules_manager_.IsFilesPolicyEnabled());
  chromeos::DlpClient::Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(DlpRulesManagerImplTest, FilesRestriction_FeatureNotEnabled) {
  // Disable feature
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDataLeakPreventionFilesRestriction);
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

  EXPECT_EQ(0, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());
  EXPECT_FALSE(dlp_rules_manager_.IsFilesPolicyEnabled());
  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, GetSourceUrlPattern) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kChatPattern);
  src_urls_1.Append(kSalesforcePattern);
  src_urls_1.Append(kDocsPattern);
  src_urls_1.Append(kDrivePattern);
  src_urls_1.Append(kCompanyPattern);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Block screenshots", "Block screenshots of work urls",
      std::move(src_urls_1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kWildCardMatching);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrintingRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Block any printing", "Block printing any docs", std::move(src_urls_2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  UpdatePolicyPref(std::move(rules));

  EXPECT_EQ(std::string(kChatPattern),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(std::string("https://") + std::string(kChatPattern)),
                DlpRulesManager::Restriction::kScreenshot,
                DlpRulesManager::Level::kBlock));
  EXPECT_EQ(std::string(kSalesforcePattern),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(std::string("https://") + std::string(kSalesforcePattern) +
                     std::string("/xyz")),
                DlpRulesManager::Restriction::kScreenshot,
                DlpRulesManager::Level::kBlock));
  EXPECT_EQ(std::string(kDocsPattern),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(std::string("https://") + std::string(kDocsPattern) +
                     std::string("/path?v=1")),
                DlpRulesManager::Restriction::kScreenshot,
                DlpRulesManager::Level::kBlock));
  EXPECT_EQ(std::string(""),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(std::string("https://") + std::string(kDrivePattern)),
                DlpRulesManager::Restriction::kScreenshot,
                DlpRulesManager::Level::kAllow));
  EXPECT_EQ(std::string(""),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(std::string("https://") + std::string(kCompanyPattern)),
                DlpRulesManager::Restriction::kPrivacyScreen,
                DlpRulesManager::Level::kBlock));
  EXPECT_EQ(std::string(kWildCardMatching),
            dlp_rules_manager_.GetSourceUrlPattern(
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kPrinting,
                DlpRulesManager::Level::kBlock));
}

TEST_F(DlpRulesManagerImplTest, ReportPriority) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls_1(base::Value::Type::LIST);
  src_urls_1.Append(kWildCardMatching);

  base::Value restrictions_1(base::Value::Type::LIST);
  restrictions_1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kReportLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Report screensharing", "Report any screensharing", std::move(src_urls_1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_1)));

  base::Value src_urls_2(base::Value::Type::LIST);
  src_urls_2.Append(kDrivePattern);
  src_urls_2.Append(kDocsPattern);

  base::Value restrictions_2(base::Value::Type::LIST);
  restrictions_2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kBlockLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Block screensharing", "Block screensharing of company urls",
      std::move(src_urls_2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_2)));

  base::Value src_urls_3(base::Value::Type::LIST);
  src_urls_3.Append(kChatPattern);

  base::Value restrictions_3(base::Value::Type::LIST);
  restrictions_3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kAllowLevel));

  rules.Append(dlp_test_util::CreateRule(
      "Allow screensharing", "Allow screensharing for chat urls",
      std::move(src_urls_3),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions_3)));

  UpdatePolicyPref(std::move(rules));

  // Screensharing from chat.google should be allowed.
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_.IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                DlpRulesManager::Restriction::kScreenShare));

  // Screensharing from docs/drive urls should be blocked.
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_.IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                DlpRulesManager::Restriction::kScreenShare));

  // Screensharing from gmail/example/Salesforce urls should be reported.
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_.IsRestricted(
                GURL(kGmailUrl), DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_.IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_.IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kScreenShare));
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_NoMatch) {
  auto result = dlp_rules_manager_.GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);

  EXPECT_TRUE(result.empty());
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_GetAggregatedDestinations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kExampleUrl);
  base::Value dst_urls1(base::Value::Type::LIST);
  // Duplicates should be ignored.
  dst_urls1.Append(kGoogleUrl);
  dst_urls1.Append(kGoogleUrl);
  dst_urls1.Append(kCompanyUrl);
  dst_urls1.Append(kGmailUrl);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Files", std::move(src_urls1), std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kExampleUrl);
  base::Value dst_urls2(base::Value::Type::LIST);
  dst_urls2.Append(kGmailUrl);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kAllowLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Explicit Allow Files", std::move(src_urls2),
      std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  UpdatePolicyPref(std::move(rules));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(dlp_rules_manager_.IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_.GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kFiles);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kGoogleUrl);
  expected[DlpRulesManager::Level::kBlock].insert(kCompanyUrl);
  expected[DlpRulesManager::Level::kAllow].insert(kGmailUrl);

  EXPECT_EQ(result, expected);

  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest,
       FilesRestriction_GetAggregatedDestinations_Wildcard) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kExampleUrl);
  base::Value dst_urls1(base::Value::Type::LIST);
  dst_urls1.Append(kWildCardMatching);
  // Since there is a wildcard, all specific destinations will be ignored.
  dst_urls1.Append(kCompanyUrl);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Files for all destinations", std::move(src_urls1),
      std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  UpdatePolicyPref(std::move(rules));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(dlp_rules_manager_.IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_.GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kFiles);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kWildCardMatching);

  EXPECT_EQ(result, expected);

  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_MixedLevels) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kExampleUrl);
  base::Value dst_urls1(base::Value::Type::LIST);
  dst_urls1.Append(kCompanyUrl);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Clipboard", std::move(src_urls1), std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kExampleUrl);
  base::Value dst_urls2(base::Value::Type::LIST);
  // Ignored because of a block restriction for the same destination.
  dst_urls2.Append(kCompanyUrl);
  dst_urls2.Append(kGmailUrl);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kWarnLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Warn Clipboard", std::move(src_urls2), std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  base::Value src_urls3(base::Value::Type::LIST);
  src_urls3.Append(kExampleUrl);
  base::Value dst_urls3(base::Value::Type::LIST);
  dst_urls3.Append(kGoogleUrl);
  base::Value restrictions3(base::Value::Type::LIST);
  restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kReportLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Report Clipboard", std::move(src_urls3), std::move(dst_urls3),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions3)));

  UpdatePolicyPref(std::move(rules));

  auto result = dlp_rules_manager_.GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kCompanyUrl);
  expected[DlpRulesManager::Level::kWarn].insert(kGmailUrl);
  expected[DlpRulesManager::Level::kReport].insert(kGoogleUrl);

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_MixedWithWildcard) {
  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kExampleUrl);
  base::Value dst_urls1(base::Value::Type::LIST);
  dst_urls1.Append(kCompanyUrl);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Clipboard", std::move(src_urls1), std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kExampleUrl);
  base::Value dst_urls2(base::Value::Type::LIST);
  dst_urls2.Append(kWildCardMatching);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kWarnLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Warn Clipboard", std::move(src_urls2), std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  base::Value src_urls3(base::Value::Type::LIST);
  src_urls3.Append(kExampleUrl);
  base::Value dst_urls3(base::Value::Type::LIST);
  // Ignored because of "*" at warn level.
  dst_urls3.Append(kGoogleUrl);
  dst_urls3.Append(kWildCardMatching);
  base::Value restrictions3(base::Value::Type::LIST);
  restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kReportLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Report Clipboard", std::move(src_urls3), std::move(dst_urls3),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions3)));

  UpdatePolicyPref(std::move(rules));

  auto result = dlp_rules_manager_.GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kCompanyUrl);
  expected[DlpRulesManager::Level::kWarn].insert(kWildCardMatching);

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedComponents_NoMatch) {
  auto result = dlp_rules_manager_.GetAggregatedComponents(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<DlpRulesManager::Component>>
      expected;
  for (auto component : DlpRulesManager::components) {
    expected[DlpRulesManager::Level::kAllow].insert(component);
  }

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_GetAggregatedComponents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);
  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append("ARC");
  dst_components.Append("CROSTINI");
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Files", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(dlp_rules_manager_.IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_.GetAggregatedComponents(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kFiles);
  std::map<DlpRulesManager::Level, std::set<DlpRulesManager::Component>>
      expected;
  expected[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kArc);
  expected[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kCrostini);
  expected[DlpRulesManager::Level::kAllow].insert(
      DlpRulesManager::Component::kPluginVm);
  expected[DlpRulesManager::Level::kAllow].insert(
      DlpRulesManager::Component::kUsb);
  expected[DlpRulesManager::Level::kAllow].insert(
      DlpRulesManager::Component::kDrive);

  EXPECT_EQ(result, expected);

  chromeos::DlpClient::Shutdown();
}

// This is a test for the crash on the login screen for files policy rule with
// no url destinations crbug.com/1358504.
TEST_F(DlpRulesManagerImplTest, SetFilesPolicyWithOnlyComponents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kExampleUrl);
  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append("ARC");
  dst_components.Append("CROSTINI");
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kFilesRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Files", std::move(src_urls), absl::nullopt,
      std::move(dst_components), std::move(restrictions)));

  UpdatePolicyPref(std::move(rules));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(dlp_rules_manager_.IsFilesPolicyEnabled());
  EXPECT_EQ(chromeos::DlpClient::Get()
                ->GetTestInterface()
                ->GetSetDlpFilesPolicyCount(),
            1);

  chromeos::DlpClient::Shutdown();
}

}  // namespace policy
