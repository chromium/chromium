// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
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
constexpr char kDriveUrl[] = "https://drive.google.com";
constexpr char kOneDriveUrl[] = "https://onedrive.live.com";

constexpr char kHttpsPrefix[] = "https://www.";

constexpr char kChatPattern[] = "chat.google.com";
constexpr char kSalesforcePattern[] = "salesforce.com";
constexpr char kDocsPattern[] = "docs.google.com";
constexpr char kDrivePattern[] = "drive.google.com";
constexpr char kCompanyPattern[] = ".company.com";
constexpr char kGooglePattern[] = "google.com";
constexpr char kMailPattern[] = "mail.google.com";
constexpr char kOneDrivePattern[] = "onedrive.live.com";

constexpr char kWrongRestriction[] = "WrongRestriction";
constexpr char kWrongComponent[] = "WrongComponent";
constexpr char kWrongLevel[] = "kWrongLevel";

constexpr char kRuleId1[] = "testid1";
constexpr char kRuleId2[] = "testid2";
constexpr char kRuleId3[] = "testid3";

constexpr char kRuleName1[] = "rule #1";
constexpr char kRuleName2[] = "rule #2";
constexpr char kRuleName3[] = "rule #3";
class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state, Profile* profile)
      : DlpRulesManagerImpl(local_state, profile) {}

  ~MockDlpRulesManager() override { Shutdown(); }
};

}  // namespace

class DlpRulesManagerImplTest : public testing::Test {
 protected:
  DlpRulesManagerImplTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    dlp_rules_manager_ = std::make_unique<MockDlpRulesManager>(
        testing_local_state_.Get(), profile_.get());

    // THe histogram tester should be created after the rules manager, since the
    // rules manager constructor call OnPolicyUpdate, and we would then record
    // that additional call.
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void UpdatePolicyPref(const std::vector<dlp_test_util::DlpRule>& rules) {
    base::Value::List policy_rules;
    for (const auto& rule : rules) {
      policy_rules.Append(rule.Create());
    }
    testing_local_state_.Get()->SetList(policy_prefs::kDlpRulesList,
                                        std::move(policy_rules));
  }

  void CheckIsRestrictedComponent(
      const std::string& src_url,
      data_controls::Component dst_component,
      DlpRulesManager::Restriction restriction,
      DlpRulesManager::Level expected_level,
      const std::string& expected_src_pattern,
      const DlpRulesManager::RuleMetadata& expected_rule_metadata) {
    std::string src_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    EXPECT_EQ(expected_level, dlp_rules_manager_->IsRestrictedComponent(
                                  GURL(src_url), dst_component, restriction,
                                  &src_pattern, &rule_metadata));
    EXPECT_EQ(src_pattern, expected_src_pattern);
    EXPECT_EQ(rule_metadata.name, expected_rule_metadata.name);
    EXPECT_EQ(rule_metadata.obfuscated_id,
              expected_rule_metadata.obfuscated_id);
  }

  void CheckIsRestrictedDestination(
      const std::string& src_url,
      const std::string& dst_url,
      DlpRulesManager::Restriction restriction,
      DlpRulesManager::Level expected_level,
      const std::string& expected_src_pattern,
      const std::string& expected_dst_pattern,
      const DlpRulesManager::RuleMetadata& expected_rule_metadata) {
    std::string src_pattern;
    std::string dst_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    EXPECT_EQ(expected_level, dlp_rules_manager_->IsRestrictedDestination(
                                  GURL(src_url), GURL(dst_url), restriction,
                                  &src_pattern, &dst_pattern, &rule_metadata));
    EXPECT_EQ(src_pattern, expected_src_pattern);
    EXPECT_EQ(dst_pattern, expected_dst_pattern);
    EXPECT_EQ(rule_metadata.name, expected_rule_metadata.name);
    EXPECT_EQ(rule_metadata.obfuscated_id,
              expected_rule_metadata.obfuscated_id);
  }

  void CheckIsRestrictedByAnyRule(
      const std::string& src_url,
      DlpRulesManager::Restriction restriction,
      DlpRulesManager::Level expected_level,
      const std::string& expected_src_pattern,
      const DlpRulesManager::RuleMetadata& expected_rule_metadata) {
    std::string src_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    EXPECT_EQ(expected_level,
              dlp_rules_manager_->IsRestrictedByAnyRule(
                  GURL(src_url), restriction, &src_pattern, &rule_metadata));
    EXPECT_EQ(src_pattern, expected_src_pattern);
    EXPECT_EQ(rule_metadata.name, expected_rule_metadata.name);
    EXPECT_EQ(rule_metadata.obfuscated_id,
              expected_rule_metadata.obfuscated_id);
  }

  void CheckGetSourceUrlPattern(
      const std::string& src_url,
      DlpRulesManager::Restriction restriction,
      DlpRulesManager::Level level,
      const std::string& expected_pattern,
      const DlpRulesManager::RuleMetadata& expected_rule_metadata) {
    DlpRulesManager::RuleMetadata rule_metadata;
    EXPECT_EQ(expected_pattern,
              dlp_rules_manager_->GetSourceUrlPattern(
                  GURL(src_url), restriction, level, &rule_metadata));
    EXPECT_EQ(rule_metadata.name, expected_rule_metadata.name);
    EXPECT_EQ(rule_metadata.obfuscated_id,
              expected_rule_metadata.obfuscated_id);
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockDlpRulesManager> dlp_rules_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::RunLoop run_loop_;
};

TEST_F(DlpRulesManagerImplTest, EmptyPref) {
  UpdatePolicyPref({});

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kPrinting));

  CheckIsRestrictedDestination(
      kExampleUrl, kGoogleUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, /*expected_src_pattern=*/"",
      /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));

  histogram_tester_->ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDlpPolicyPresentUMA,
      false, 1);
}

TEST_F(DlpRulesManagerImplTest, UnknownRestriction) {
  dlp_test_util::DlpRule rule(kRuleName1, "Unknown", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(kWrongRestriction, data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  histogram_tester_->ExpectBucketCount(
      "Enterprise.Dlp.RestrictionConfigured",
      DlpRulesManager::Restriction::kUnknownRestriction, 0);
}

// Unknown Components should be allowed, (b/267622459).
TEST_F(DlpRulesManagerImplTest, UnknownComponent) {
  dlp_test_util::DlpRule rule(kRuleName1, "Unknown", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstComponent(kWrongComponent)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  histogram_tester_->ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                       DlpRulesManager::Restriction::kClipboard,
                                       1);

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kUnknownComponent,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      /*expected_src_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

TEST_F(DlpRulesManagerImplTest, UnknownLevel) {
  dlp_test_util::DlpRule rule(kRuleName1, "Unknown", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard, kWrongLevel);

  UpdatePolicyPref({rule});

  histogram_tester_->ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                       DlpRulesManager::Restriction::kClipboard,
                                       0);
}

TEST_F(DlpRulesManagerImplTest, BlockPriority) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock)
      .AddRestriction(data_controls::kRestrictionScreenshot,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Exceptional allow", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kGoogleUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelAllow);

  UpdatePolicyPref({rule1, rule2});

  CheckIsRestrictedDestination(
      kExampleUrl, kGoogleUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, kExampleUrl, kGoogleUrl,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  CheckIsRestrictedDestination(
      kExampleUrl, kGmailUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kBlock, kExampleUrl, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  CheckIsRestrictedByAnyRule(
      kExampleUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kBlock, kExampleUrl,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  histogram_tester_->ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDlpPolicyPresentUMA,
      true, 1);
  histogram_tester_->ExpectBucketCount("Enterprise.Dlp.RestrictionConfigured",
                                       DlpRulesManager::Restriction::kClipboard,
                                       2);
  histogram_tester_->ExpectBucketCount(
      "Enterprise.Dlp.RestrictionConfigured",
      DlpRulesManager::Restriction::kScreenshot, 1);

  // Clear pref
  UpdatePolicyPref({});

  CheckIsRestrictedDestination(
      kExampleUrl, kGoogleUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, /*expected_src_pattern=*/"",
      /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));

  CheckIsRestrictedDestination(
      kExampleUrl, kGmailUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, /*expected_src_pattern=*/"",
      /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerImplTest, UpdatePref) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionScreenshot,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule1});

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  dlp_test_util::DlpRule rule2(kRuleName2, "Exceptional allow", kRuleId2);
  rule2.AddSrcUrl(kGoogleUrl)
      .AddRestriction(data_controls::kRestrictionScreenshot,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule2});

  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kGoogleUrl), DlpRulesManager::Restriction::kScreenshot));
}

TEST_F(DlpRulesManagerImplTest, IsRestrictedComponent_Clipboard) {
  dlp_test_util::DlpRule rule(kRuleName1, "Block", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstComponent(data_controls::kArc)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kArc,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kExampleUrl, DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kCrostini,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      /*expected_src_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

TEST_F(DlpRulesManagerImplTest,
       RestrictedComponentsRestrictsAssociatedUrls_Files) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  dlp_test_util::DlpRule rule(kRuleName1, "Block", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstComponent(data_controls::kDrive)
      .AddDstComponent(data_controls::kOneDrive)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kDrive,
      DlpRulesManager::Restriction::kFiles, DlpRulesManager::Level::kBlock,
      kExampleUrl, DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kOneDrive,
      DlpRulesManager::Restriction::kFiles, DlpRulesManager::Level::kBlock,
      kExampleUrl, DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  // Make sure that blocking the components also blocks their associated
  // website.
  CheckIsRestrictedDestination(
      kExampleUrl, kDriveUrl, DlpRulesManager::Restriction::kFiles,
      DlpRulesManager::Level::kBlock, kExampleUrl, kDrivePattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      kExampleUrl, kOneDriveUrl, DlpRulesManager::Restriction::kFiles,
      DlpRulesManager::Level::kBlock, kExampleUrl, kOneDrivePattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());

  run_loop_.Run();

  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, SameSrcDst_Clipboard) {
  dlp_test_util::DlpRule rule(kRuleName1, "Block", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  CheckIsRestrictedDestination(
      kExampleUrl, kExampleUrl, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, /*expected_src_pattern=*/"",
      /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

TEST_F(DlpRulesManagerImplTest, EmptyUrl_Clipboard) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Block", kRuleId2);
  rule2.AddSrcUrl(kGmailUrl)
      .AddDstUrl(kGoogleUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule1, rule2});

  CheckIsRestrictedDestination(
      kExampleUrl, /*dst_url=*/"", DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kBlock, kExampleUrl, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      kGmailUrl, /*dst_url=*/"", DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow, /*expected_src_pattern=*/"",
      /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

TEST_F(DlpRulesManagerImplTest, IsRestricted_MultipleURLs) {
  const std::vector<std::string> urls = {kChatPattern, kSalesforcePattern,
                                         kDocsPattern, kDrivePattern,
                                         kCompanyPattern};

  dlp_test_util::DlpRule rule1(
      kRuleName1, "Allow copy and paste for work purposes", kRuleId1);
  for (const std::string& url : urls) {
    rule1.AddSrcUrl(url).AddDstUrl(url);
  }
  rule1.AddRestriction(data_controls::kRestrictionClipboard,
                       data_controls::kLevelAllow);

  dlp_test_util::DlpRule rule2(
      kRuleName2, "Disallow copy and paste for non-work purposes", kRuleId2);
  for (const std::string& url : urls) {
    rule2.AddSrcUrl(url);
  }
  rule2.AddDstUrl(kWildCardMatching);
  rule2.AddRestriction(data_controls::kRestrictionClipboard,
                       data_controls::kLevelBlock);

  UpdatePolicyPref({rule1, rule2});

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kChatPattern}),
      base::StrCat({kHttpsPrefix, kSalesforcePattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      kChatPattern, kSalesforcePattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kDocsPattern}),
      base::StrCat({kHttpsPrefix, kDrivePattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      kDocsPattern, kDrivePattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      kCompanyUrl, base::StrCat({kHttpsPrefix, kSalesforcePattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      kCompanyPattern, kSalesforcePattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kSalesforcePattern}),
      base::StrCat({kHttpsPrefix, kDocsPattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      kSalesforcePattern, kDocsPattern,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kChatPattern}), kGoogleUrl,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kChatPattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kSalesforcePattern}), kExampleUrl,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kSalesforcePattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kDocsPattern}), kGoogleUrl,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kDocsPattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kDrivePattern}), kExampleUrl,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kDrivePattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));
}

TEST_F(DlpRulesManagerImplTest, DisabledByFeature) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock)
      .AddRestriction(data_controls::kRestrictionScreenshot,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule1});

  CheckIsRestrictedDestination(
      kExampleUrl, kWildCardMatching, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kBlock, kExampleUrl, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenshot));

  // Disable feature
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDataLeakPreventionPolicy);

  dlp_test_util::DlpRule rule2(kRuleName2, "Block", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule2});

  CheckIsRestrictedDestination(
      kExampleUrl, kWildCardMatching, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kAllow,
      /*expected_src_pattern=*/"", /*expected_dst_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

TEST_F(DlpRulesManagerImplTest, WarnPriority) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Warn on every copy from google.com",
                               kRuleId1);
  rule1.AddSrcUrl(kGooglePattern)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelWarn);

  dlp_test_util::DlpRule rule2(
      kRuleName2, "Block copy/paste from docs, drive, gmail", kRuleId2);
  rule2.AddSrcUrl(kDocsPattern)
      .AddSrcUrl(kDrivePattern)
      .AddSrcUrl(kMailPattern)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule3(
      kRuleName3, "Allow copy/paste inside docs, drive, gmail", kRuleId3);
  rule3.AddSrcUrl(kDocsPattern)
      .AddSrcUrl(kDrivePattern)
      .AddSrcUrl(kMailPattern)
      .AddDstUrl(kDocsPattern)
      .AddDstUrl(kDrivePattern)
      .AddDstUrl(kMailPattern)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelAllow);

  UpdatePolicyPref({rule1, rule2, rule3});

  // Copy/paste from chat.google to example.com should be warned.
  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kChatPattern}), kExampleUrl,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kWarn,
      kGooglePattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  // Copy/paste from docs to salesforce should be blocked.
  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kDocsPattern}),
      base::StrCat({kHttpsPrefix, kSalesforcePattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kDocsPattern, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  // Copy/paste from docs to gmail should be allowed.
  CheckIsRestrictedDestination(
      base::StrCat({kHttpsPrefix, kDocsPattern}),
      base::StrCat({kHttpsPrefix, kMailPattern}),
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kAllow,
      kDocsPattern, kMailPattern,
      DlpRulesManager::RuleMetadata(kRuleName3, kRuleId3));
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_DlpClientNotified) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  EXPECT_EQ(0, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());

  dlp_test_util::DlpRule rule(kRuleName1, "Block Files", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  EXPECT_EQ(1, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());
  EXPECT_TRUE(dlp_rules_manager_->IsFilesPolicyEnabled());

  dlp_rules_manager_->DlpDaemonRestarted();

  // The above call to DlpRulesManagerImpl::DlpDaemonRestarted posts a task to
  // the same task runner as the one used here. Doing this ensures that the call
  // chromeos::DlpClient::Shutdown() does not happen before the task is
  // completed. The same approach is used on multiple other tests in this file.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());

  EXPECT_EQ(2, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());
  run_loop_.Run();

  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_FeatureNotEnabled) {
  // Disable feature
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  EXPECT_EQ(0, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());

  dlp_test_util::DlpRule rule(kRuleName1, "Block Files", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  EXPECT_EQ(0, chromeos::DlpClient::Get()
                   ->GetTestInterface()
                   ->GetSetDlpFilesPolicyCount());
  EXPECT_FALSE(dlp_rules_manager_->IsFilesPolicyEnabled());
  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, GetSourceUrlPattern) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block screenshots of work urls",
                               kRuleId1);
  rule1.AddSrcUrl(kChatPattern)
      .AddSrcUrl(kSalesforcePattern)
      .AddSrcUrl(kDocsPattern)
      .AddSrcUrl(kDrivePattern)
      .AddSrcUrl(kCompanyPattern)
      .AddRestriction(data_controls::kRestrictionScreenshot,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Block printing any docs", kRuleId2);
  rule2.AddSrcUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule1, rule2});

  CheckGetSourceUrlPattern(std::string("https://") + std::string(kChatPattern),
                           DlpRulesManager::Restriction::kScreenshot,
                           DlpRulesManager::Level::kBlock, kChatPattern,
                           DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckGetSourceUrlPattern(
      std::string("https://") + std::string(kSalesforcePattern) +
          std::string("/xyz"),
      DlpRulesManager::Restriction::kScreenshot, DlpRulesManager::Level::kBlock,
      kSalesforcePattern, DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckGetSourceUrlPattern(std::string("https://") + std::string(kDocsPattern) +
                               std::string("/path?v=1"),
                           DlpRulesManager::Restriction::kScreenshot,
                           DlpRulesManager::Level::kBlock, kDocsPattern,
                           DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckGetSourceUrlPattern(
      std::string("https://") + std::string(kDrivePattern),
      DlpRulesManager::Restriction::kScreenshot, DlpRulesManager::Level::kAllow,
      /*expected_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));

  CheckGetSourceUrlPattern(
      std::string("https://") + std::string(kCompanyPattern),
      DlpRulesManager::Restriction::kPrivacyScreen,
      DlpRulesManager::Level::kAllow, /*expected_pattern=*/"",
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));

  CheckGetSourceUrlPattern(kGoogleUrl, DlpRulesManager::Restriction::kPrinting,
                           DlpRulesManager::Level::kBlock, kWildCardMatching,
                           DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));
}

TEST_F(DlpRulesManagerImplTest, ReportPriority) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Report any screensharing",
                               kRuleId1);
  rule1.AddSrcUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionScreenShare,
                      data_controls::kLevelReport);

  dlp_test_util::DlpRule rule2(kRuleName2,
                               "Block screensharing of company urls", kRuleId2);
  rule2.AddSrcUrl(kDrivePattern)
      .AddSrcUrl(kDocsPattern)
      .AddRestriction(data_controls::kRestrictionScreenShare,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule3(kRuleName3, "Allow screensharing for chat urls",
                               kRuleId3);
  rule3.AddSrcUrl(kChatPattern)
      .AddRestriction(data_controls::kRestrictionScreenShare,
                      data_controls::kLevelAllow);

  UpdatePolicyPref({rule1, rule2, rule3});

  // Screensharing from chat.google should be allowed.
  EXPECT_EQ(DlpRulesManager::Level::kAllow,
            dlp_rules_manager_->IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kChatPattern})),
                DlpRulesManager::Restriction::kScreenShare));

  // Screensharing from docs/drive urls should be blocked.
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kDocsPattern})),
                DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            dlp_rules_manager_->IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kDrivePattern})),
                DlpRulesManager::Restriction::kScreenShare));

  // Screensharing from gmail/example/Salesforce urls should be reported.
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_->IsRestricted(
                GURL(kGmailUrl), DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_->IsRestricted(
                GURL(kExampleUrl), DlpRulesManager::Restriction::kScreenShare));
  EXPECT_EQ(DlpRulesManager::Level::kReport,
            dlp_rules_manager_->IsRestricted(
                GURL(base::StrCat({kHttpsPrefix, kSalesforcePattern})),
                DlpRulesManager::Restriction::kScreenShare));
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_NoMatch) {
  auto result = dlp_rules_manager_->GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);

  EXPECT_TRUE(result.empty());
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_GetAggregatedDestinations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  dlp_test_util::DlpRule rule1(kRuleName1, "Block Files", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kGoogleUrl)
      .AddDstUrl(kGoogleUrl)  // Duplicates should be ignored.
      .AddDstUrl(kCompanyUrl)
      .AddDstUrl(kGmailUrl)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Explicit Allow Files", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kGmailUrl)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelAllow);

  UpdatePolicyPref({rule1, rule2});

  // See call to PostTask in a test above for more detail. In this case,
  // UpdatePolicyPref posts the task to the same task runner.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(dlp_rules_manager_->IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_->GetAggregatedDestinations(
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

  dlp_test_util::DlpRule rule(kRuleName1, "Block Files for all destinations",
                              kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddDstUrl(kCompanyUrl)  // Since there is a wildcard, all specific
                               // destinations will be ignored.
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  // See call to PostTask in a test above for more detail.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(dlp_rules_manager_->IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_->GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kFiles);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kWildCardMatching);

  EXPECT_EQ(result, expected);

  chromeos::DlpClient::Shutdown();
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_MixedLevels) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block Clipboard", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kCompanyUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Warn Clipboard", kRuleId1);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kCompanyUrl)  // Ignored because of a block restriction for the
                               // same destination.
      .AddDstUrl(kGmailUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelWarn);

  dlp_test_util::DlpRule rule3(kRuleName3, "Report Clipboard", kRuleId3);
  rule3.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kGoogleUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelReport);

  UpdatePolicyPref({rule1, rule2, rule3});

  auto result = dlp_rules_manager_->GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kCompanyUrl);
  expected[DlpRulesManager::Level::kWarn].insert(kGmailUrl);
  expected[DlpRulesManager::Level::kReport].insert(kGoogleUrl);

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedDestinations_MixedWithWildcard) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block Clipboard", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kCompanyUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Warn Clipboard", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelWarn);

  dlp_test_util::DlpRule rule3(kRuleName3, "Report Clipboard", kRuleId3);
  rule3.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kGoogleUrl)  // Ignored because of "*" at warn level.
      .AddDstUrl(kWildCardMatching)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelReport);

  UpdatePolicyPref({rule1, rule2, rule3});

  auto result = dlp_rules_manager_->GetAggregatedDestinations(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<std::string>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(kCompanyUrl);
  expected[DlpRulesManager::Level::kWarn].insert(kWildCardMatching);

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, GetAggregatedComponents_NoMatch) {
  auto result = dlp_rules_manager_->GetAggregatedComponents(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kClipboard);
  std::map<DlpRulesManager::Level, std::set<data_controls::Component>> expected;
  for (auto component : data_controls::kAllComponents) {
    expected[DlpRulesManager::Level::kAllow].insert(component);
  }

  EXPECT_EQ(result, expected);
}

TEST_F(DlpRulesManagerImplTest, FilesRestriction_GetAggregatedComponents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  dlp_test_util::DlpRule rule(kRuleName1, "Block Files", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstComponent(data_controls::kArc)
      .AddDstComponent(data_controls::kCrostini)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  // See call to PostTask in a test above for more detail.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(dlp_rules_manager_->IsFilesPolicyEnabled());

  auto result = dlp_rules_manager_->GetAggregatedComponents(
      GURL(kExampleUrl), DlpRulesManager::Restriction::kFiles);
  std::map<DlpRulesManager::Level, std::set<data_controls::Component>> expected;
  expected[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kArc);
  expected[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kCrostini);
  expected[DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kPluginVm);
  expected[DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kUsb);
  expected[DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kDrive);
  expected[DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kOneDrive);

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

  dlp_test_util::DlpRule rule(kRuleName1, "Block Files", kRuleId1);
  rule.AddSrcUrl(kExampleUrl)
      .AddDstComponent(data_controls::kArc)
      .AddDstComponent(data_controls::kCrostini)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule});

  // See call to PostTask in a test above for more detail.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(dlp_rules_manager_->IsFilesPolicyEnabled());
  EXPECT_EQ(chromeos::DlpClient::Get()
                ->GetTestInterface()
                ->GetSetDlpFilesPolicyCount(),
            1);

  chromeos::DlpClient::Shutdown();
}

// Test that returned rule metadata is empty if name is set, but id is unset.
TEST_F(DlpRulesManagerImplTest, EmptyMetadataReportedIfRuleidUnset) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block Printing", std::string());
  rule1.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock);
  UpdatePolicyPref({rule1});

  CheckGetSourceUrlPattern(
      kExampleUrl, DlpRulesManager::Restriction::kPrinting,
      DlpRulesManager::Level::kBlock, kExampleUrl,
      DlpRulesManager::RuleMetadata(/*name=*/"", /*obfuscated_id=*/""));
}

// Test that after policy refresh, the correct metadata is returned
TEST_F(DlpRulesManagerImplTest, MetadataMapEmptiedAfterPolicyUpdate) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block Printing", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock);
  UpdatePolicyPref({rule1});

  dlp_test_util::DlpRule rule2(kRuleName2, "Block Printing", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock);
  UpdatePolicyPref({rule2});

  CheckGetSourceUrlPattern(kExampleUrl, DlpRulesManager::Restriction::kPrinting,
                           DlpRulesManager::Level::kBlock, kExampleUrl,
                           DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));
}

// Test that for overlapping rules with same restriction, metadata of the first
// rule in the policy is reported.
TEST_F(DlpRulesManagerImplTest, TestOrderSameLevelPrinting) {
  dlp_test_util::DlpRule rule1(kRuleName1, "Block Printing", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule2(kRuleName2, "Block Printing and copy paste",
                               kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddDstComponent(data_controls::kCrostini)
      .AddRestriction(data_controls::kRestrictionPrinting,
                      data_controls::kLevelBlock)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock);

  dlp_test_util::DlpRule rule3(kRuleName3, "Block Screenshare and copy paste",
                               kRuleId3);
  rule3.AddSrcUrl(kExampleUrl)
      .AddDstUrl(kWildCardMatching)
      .AddDstComponent(data_controls::kCrostini)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelBlock)
      .AddRestriction(data_controls::kRestrictionScreenShare,
                      data_controls::kLevelBlock);

  UpdatePolicyPref({rule1, rule2, rule3});

  CheckGetSourceUrlPattern(kExampleUrl, DlpRulesManager::Restriction::kPrinting,
                           DlpRulesManager::Level::kBlock, kExampleUrl,
                           DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedByAnyRule(
      kExampleUrl, DlpRulesManager::Restriction::kPrinting,
      DlpRulesManager::Level::kBlock, kExampleUrl,
      DlpRulesManager::RuleMetadata(kRuleName1, kRuleId1));

  CheckIsRestrictedDestination(
      kExampleUrl, kCompanyPattern, DlpRulesManager::Restriction::kClipboard,
      DlpRulesManager::Level::kBlock, kExampleUrl, kWildCardMatching,
      DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));

  CheckIsRestrictedComponent(
      kExampleUrl, data_controls::Component::kCrostini,
      DlpRulesManager::Restriction::kClipboard, DlpRulesManager::Level::kBlock,
      kExampleUrl, DlpRulesManager::RuleMetadata(kRuleName2, kRuleId2));
}

// Tests creation and deletion of DataTransferDlpController.
TEST_F(DlpRulesManagerImplTest, DataTransferDlpController) {
  // There should be no instance given no rule is set yet.
  EXPECT_FALSE(ui::DataTransferPolicyController::HasInstance());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataLeakPreventionFilesRestriction);
  chromeos::DlpClient::InitializeFake();

  // Set only clipboard restriction, DataTransferDlpController should be
  // instantiated.
  dlp_test_util::DlpRule rule1(kRuleName1, "Report Clipboard", kRuleId1);
  rule1.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionClipboard,
                      data_controls::kLevelReport)
      .AddDstUrl(kChatPattern);

  UpdatePolicyPref({rule1});
  EXPECT_TRUE(DataTransferDlpController::HasInstance());

  // Remove the restrictions, DataTransferDlpController instance
  // should be deleted.
  UpdatePolicyPref({});
  EXPECT_FALSE(DataTransferDlpController::HasInstance());

  // Set only files restriction, DataTransferDlpController should be
  // instantiated.
  dlp_test_util::DlpRule rule2(kRuleName2, "Warn Files", kRuleId2);
  rule2.AddSrcUrl(kExampleUrl)
      .AddRestriction(data_controls::kRestrictionFiles,
                      data_controls::kLevelWarn)
      .AddDstUrl(kChatPattern);

  UpdatePolicyPref({rule2});
  EXPECT_TRUE(DataTransferDlpController::HasInstance());

  // Remove the restrictions, DataTransferDlpController instance
  // should be deleted.
  UpdatePolicyPref({});
  EXPECT_FALSE(DataTransferDlpController::HasInstance());

  // Set clipboard and files restrictions, DataTransferDlpController
  // should be instantiated.
  UpdatePolicyPref({rule1, rule2});
  EXPECT_TRUE(DataTransferDlpController::HasInstance());

  // See call to PostTask in a test above for more detail.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  chromeos::DlpClient::Shutdown();
}

}  // namespace policy
