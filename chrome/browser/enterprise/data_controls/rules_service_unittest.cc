// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/rules_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/data_controls/test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

class DataControlsRulesServiceTest : public testing::Test {
 public:
  explicit DataControlsRulesServiceTest(bool feature_enabled = true)
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    if (feature_enabled) {
      scoped_features_.InitAndEnableFeature(kEnableDesktopDataControls);
    } else {
      scoped_features_.InitAndDisableFeature(kEnableDesktopDataControls);
    }
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

class DataControlsRulesServiceFeatureDisabledTest
    : public DataControlsRulesServiceTest {
 public:
  DataControlsRulesServiceFeatureDisabledTest()
      : DataControlsRulesServiceTest(false) {}
};

}  // namespace

TEST_F(DataControlsRulesServiceFeatureDisabledTest, NoVerdicts) {
  SetDataControls(profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "BLOCK"}
                    ]
                  })"});
  auto verdict = RulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->GetPrintVerdict(GURL("https://google.com"));
  ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
  ASSERT_TRUE(verdict.TakeInitialReportClosure().is_null());
  ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());
}

TEST_F(DataControlsRulesServiceTest, GetPrintVerdict_URL) {
  GURL google_url = GURL("https://google.com");

  auto verdict = RulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->GetPrintVerdict(google_url);
  ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
  ASSERT_TRUE(verdict.TakeInitialReportClosure().is_null());
  ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());

  SetDataControls(profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "BLOCK"}
                    ]
                  })"});
  verdict = RulesServiceFactory::GetInstance()
                ->GetForBrowserContext(profile())
                ->GetPrintVerdict(google_url);
  ASSERT_EQ(verdict.level(), Rule::Level::kBlock);
  ASSERT_FALSE(verdict.TakeInitialReportClosure().is_null());
  ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());

  SetDataControls(profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "WARN"}
                    ]
                  })"});
  verdict = RulesServiceFactory::GetInstance()
                ->GetForBrowserContext(profile())
                ->GetPrintVerdict(google_url);
  ASSERT_EQ(verdict.level(), Rule::Level::kWarn);
  ASSERT_FALSE(verdict.TakeInitialReportClosure().is_null());
  ASSERT_FALSE(verdict.TakeBypassReportClosure().is_null());

  SetDataControls(profile()->GetPrefs(), {
                                             R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "ALLOW"}
                    ]
                  })",
                                             R"({
                    "sources": {
                      "urls": ["https://*"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "WARN"}
                    ]
                  })"});
  verdict = RulesServiceFactory::GetInstance()
                ->GetForBrowserContext(profile())
                ->GetPrintVerdict(google_url);
  ASSERT_EQ(verdict.level(), Rule::Level::kAllow);
  ASSERT_TRUE(verdict.TakeInitialReportClosure().is_null());
  ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());
}

}  // namespace data_controls
