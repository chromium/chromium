// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/enterprise/data_controls/test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/action_context.h"
#include "components/enterprise/data_controls/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kTestClipboardBlockRule[] = R"({
  "sources": {
    "urls": [ "google.com" ]
  },
  "restrictions": [
    { "class": "CLIPBOARD", "level": "BLOCK" }
  ]
})";

constexpr char kTestClipboardWarnRule[] = R"({
  "sources": {
    "urls": [ "google.com" ]
  },
  "restrictions": [
    { "class": "CLIPBOARD", "level": "WARN" }
  ]
})";

constexpr char kTestClipboardAuditRule[] = R"({
  "sources": {
    "urls": [ "google.com" ]
  },
  "restrictions": [
    { "class": "CLIPBOARD", "level": "REPORT" }
  ]
})";

}  // namespace

class ChromeDlpRulesManagerTest : public testing::Test {
 public:
  explicit ChromeDlpRulesManagerTest(bool feature_enabled = true)
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    if (feature_enabled) {
      scoped_features_.InitAndEnableFeature(kEnableDesktopDataControls);
    } else {
      scoped_features_.InitAndDisableFeature(kEnableDesktopDataControls);
    }
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  ChromeDlpRulesManager* rules_manager() {
    if (!rules_manager_) {
      rules_manager_ = base::WrapUnique(new ChromeDlpRulesManager(profile_));
    }
    return rules_manager_.get();
  }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeDlpRulesManager> rules_manager_;
};

class ChromeDlpRulesManagerFeatureDisabledTest
    : public ChromeDlpRulesManagerTest {
 public:
  ChromeDlpRulesManagerFeatureDisabledTest()
      : ChromeDlpRulesManagerTest(false) {}
};

TEST_F(ChromeDlpRulesManagerFeatureDisabledTest, GetVerdict) {
  ActionContext context = {.source = {.url = GURL("https://google.com")}};

  SetDataControls(profile()->GetPrefs(), {kTestClipboardAuditRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kNotSet);

  SetDataControls(profile()->GetPrefs(), {kTestClipboardWarnRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kNotSet);

  SetDataControls(profile()->GetPrefs(), {kTestClipboardBlockRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kNotSet);
}

TEST_F(ChromeDlpRulesManagerTest, GetVerdictNoRules) {
  ActionContext context = {.source = {.url = GURL("https://google.com")}};

  SetDataControls(profile()->GetPrefs(), {});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kNotSet);
}

TEST_F(ChromeDlpRulesManagerTest, GetVerdictForSingleRule) {
  ActionContext context = {.source = {.url = GURL("https://google.com")}};

  SetDataControls(profile()->GetPrefs(), {kTestClipboardAuditRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kReport);

  SetDataControls(profile()->GetPrefs(), {kTestClipboardWarnRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kWarn);

  SetDataControls(profile()->GetPrefs(), {kTestClipboardBlockRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kBlock);
}

TEST_F(ChromeDlpRulesManagerTest, GetVerdictForMultipleRules) {
  ActionContext context = {.source = {.url = GURL("https://google.com")}};

  SetDataControls(profile()->GetPrefs(),
                  {kTestClipboardAuditRule, kTestClipboardWarnRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kWarn);

  SetDataControls(profile()->GetPrefs(),
                  {kTestClipboardAuditRule, kTestClipboardBlockRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kBlock);

  SetDataControls(profile()->GetPrefs(),
                  {kTestClipboardWarnRule, kTestClipboardBlockRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kBlock);

  SetDataControls(profile()->GetPrefs(),
                  {kTestClipboardAuditRule, kTestClipboardWarnRule,
                   kTestClipboardBlockRule});
  ASSERT_EQ(rules_manager()
                ->GetVerdict(Rule::Restriction::kClipboard, context)
                .level(),
            Rule::Level::kBlock);
}

}  // namespace data_controls
