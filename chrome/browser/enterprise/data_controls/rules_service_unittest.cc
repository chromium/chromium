// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/rules_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/data_controls/test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

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
    incognito_profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  Profile* profile() { return profile_; }

  Profile* incognito_profile() { return incognito_profile_; }

  content::WebContents* incognito_web_contents() {
    if (!incognito_web_contents_) {
      content::WebContents::CreateParams params(incognito_profile_);
      incognito_web_contents_ = content::WebContents::Create(params);
    }
    return incognito_web_contents_.get();
  }

  const GURL google_url() const { return GURL("https://google.com"); }

  content::ClipboardEndpoint google_url_endpoint() const {
    return content::ClipboardEndpoint(ui::DataTransferEndpoint(google_url()));
  }

  content::ClipboardEndpoint empty_endpoint() const {
    return content::ClipboardEndpoint(std::nullopt);
  }

  content::ClipboardEndpoint incognito_endpoint() {
    return content::ClipboardEndpoint(
        std::nullopt,
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(incognito_profile());
        }),
        *incognito_web_contents()->GetPrimaryMainFrame());
  }

  void ExpectBlockVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kBlock);
    ASSERT_FALSE(verdict.TakeInitialReportClosure().is_null());
    ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());
  }

  void ExpectWarnVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kWarn);
    ASSERT_FALSE(verdict.TakeInitialReportClosure().is_null());
    ASSERT_FALSE(verdict.TakeBypassReportClosure().is_null());
  }

  void ExpectAllowVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kAllow);
    ASSERT_TRUE(verdict.TakeInitialReportClosure().is_null());
    ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());
  }

  void ExpectNoVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
    ASSERT_TRUE(verdict.TakeInitialReportClosure().is_null());
    ASSERT_TRUE(verdict.TakeBypassReportClosure().is_null());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<Profile> incognito_profile_;
  std::unique_ptr<content::WebContents> incognito_web_contents_;
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
                      {"class": "PRINTING", "level": "BLOCK"},
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(RulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPrintVerdict(google_url()));
  ExpectNoVerdict(RulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPasteVerdict(
                          /*source*/ google_url_endpoint(),
                          /*destination*/ empty_endpoint(),
                          /*metadata*/ {}));
}

TEST_F(DataControlsRulesServiceTest, NoRuleSet) {
  ExpectNoVerdict(RulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPrintVerdict(google_url()));
  ExpectNoVerdict(RulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPasteVerdict(
                          /*source*/ google_url_endpoint(),
                          /*destination*/ empty_endpoint(),
                          /*metadata*/ {}));
}

TEST_F(DataControlsRulesServiceTest, SourceURL) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"},
                        {"class": "PRINTING", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPrintVerdict(google_url()));
    ExpectBlockVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "PRINTING", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(RulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPrintVerdict(google_url()));
    ExpectWarnVerdict(RulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ google_url_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"},
                        {"class": "PRINTING", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "sources": {
                        "urls": ["https://*"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "PRINTING", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPrintVerdict(google_url()));
    ExpectAllowVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
  }
}

TEST_F(DataControlsRulesServiceTest, DestinationURL) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});

    ExpectBlockVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});

    ExpectWarnVerdict(RulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ google_url_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "destinations": {
                        "urls": ["https://*"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }
}

TEST_F(DataControlsRulesServiceTest, SourceIncognito) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ incognito_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(RulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ incognito_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ incognito_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
  }
}

TEST_F(DataControlsRulesServiceTest, DestinationIncognito) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ incognito_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(RulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ incognito_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(RulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ incognito_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(RulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
  }
}

}  // namespace data_controls
