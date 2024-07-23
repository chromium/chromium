// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace data_controls {

namespace {

constexpr size_t kFirstRuleIndex = 0;
constexpr char kFirstRuleID[] = "1234";

class DataControlsRulesServiceTest : public testing::Test {
 public:
  explicit DataControlsRulesServiceTest(bool desktop_feature_enabled = true,
                                        bool screenshot_feature_enabled = true)
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (desktop_feature_enabled) {
      enabled_features.push_back(kEnableDesktopDataControls);
    } else {
      disabled_features.push_back(kEnableDesktopDataControls);
    }

    if (screenshot_feature_enabled) {
      enabled_features.push_back(kEnableScreenshotProtection);
    } else {
      disabled_features.push_back(kEnableScreenshotProtection);
    }

    scoped_features_.InitWithFeatures(enabled_features, disabled_features);

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user-1");
    other_profile_ = profile_manager_.CreateTestingProfile("test-user-2");
    incognito_profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  Profile* profile() { return profile_; }

  Profile* other_profile() { return other_profile_; }

  Profile* incognito_profile() { return incognito_profile_; }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  content::WebContents* other_profile_web_contents() {
    if (!other_profile_web_contents_) {
      content::WebContents::CreateParams params(other_profile_);
      other_profile_web_contents_ = content::WebContents::Create(params);
    }
    return other_profile_web_contents_.get();
  }

  content::WebContents* incognito_web_contents() {
    if (!incognito_web_contents_) {
      content::WebContents::CreateParams params(incognito_profile_);
      incognito_web_contents_ = content::WebContents::Create(params);
    }
    return incognito_web_contents_.get();
  }

  const GURL google_url() const { return GURL("https://google.com"); }

  content::ClipboardEndpoint google_url_endpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(google_url()),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(profile());
        }),
        *web_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint empty_endpoint() const {
    return content::ClipboardEndpoint(std::nullopt);
  }

  content::ClipboardEndpoint incognito_endpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("http://foo.com")),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(incognito_profile());
        }),
        *incognito_web_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint other_profile_endpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(google_url()),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(other_profile());
        }),
        *other_profile_web_contents()->GetPrimaryMainFrame());
  }

  void ExpectBlockVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kBlock);
    EXPECT_EQ(verdict.triggered_rules().size(), 1u);
    EXPECT_TRUE(verdict.triggered_rules().count(kFirstRuleIndex));
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_name, "block");
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_id,
              kFirstRuleID);
  }

  void ExpectWarnVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kWarn);
    EXPECT_EQ(verdict.triggered_rules().size(), 1u);
    EXPECT_TRUE(verdict.triggered_rules().count(kFirstRuleIndex));
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_name, "warn");
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_id,
              kFirstRuleID);
  }

  void ExpectAllowVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kAllow);
    EXPECT_TRUE(verdict.triggered_rules().empty());
  }

  void ExpectNoVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
    EXPECT_TRUE(verdict.triggered_rules().empty());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<TestingProfile> other_profile_;
  raw_ptr<Profile> incognito_profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::WebContents> other_profile_web_contents_;
  std::unique_ptr<content::WebContents> incognito_web_contents_;
};

class DataControlsRulesServiceDesktopFeatureDisabledTest
    : public DataControlsRulesServiceTest {
 public:
  DataControlsRulesServiceDesktopFeatureDisabledTest()
      : DataControlsRulesServiceTest(false, true) {}
};

class DataControlsRulesServiceScreenshotFeatureDisabledTest
    : public DataControlsRulesServiceTest {
 public:
  DataControlsRulesServiceScreenshotFeatureDisabledTest()
      : DataControlsRulesServiceTest(true, false) {}
};

class DataControlsRulesServiceAllFeaturesDisabledTest
    : public DataControlsRulesServiceTest {
 public:
  DataControlsRulesServiceAllFeaturesDisabledTest()
      : DataControlsRulesServiceTest(false, false) {}
};

}  // namespace

TEST_F(DataControlsRulesServiceDesktopFeatureDisabledTest,
       NoVerdictsForDesktopRestrictions) {
  SetDataControls(profile()->GetPrefs(), {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "BLOCK"},
                      {"class": "CLIPBOARD", "level": "BLOCK"},
                      {"class": "SCREENSHOT", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPrintVerdict(google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPasteVerdict(
                          /*source*/ google_url_endpoint(),
                          /*destination*/ empty_endpoint(),
                          /*metadata*/ {}));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyToOSClipboardVerdict(
                          /*source*/ google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyRestrictedBySourceVerdict(
                          /*source*/ google_url()));
  EXPECT_TRUE(ChromeRulesServiceFactory::GetInstance()
                  ->GetForBrowserContext(profile())
                  ->BlockScreenshots(google_url()));
}

TEST_F(DataControlsRulesServiceScreenshotFeatureDisabledTest,
       NoVerdictsForScreenshotRestriction) {
  SetDataControls(profile()->GetPrefs(), {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "BLOCK"},
                      {"class": "CLIPBOARD", "level": "BLOCK"},
                      {"class": "SCREENSHOT", "level": "BLOCK"}
                    ]
                  })"});
  ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                         ->GetForBrowserContext(profile())
                         ->GetPrintVerdict(google_url()));
  ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                         ->GetForBrowserContext(profile())
                         ->GetPasteVerdict(
                             /*source*/ google_url_endpoint(),
                             /*destination*/ empty_endpoint(),
                             /*metadata*/ {}));
  ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                         ->GetForBrowserContext(profile())
                         ->GetCopyToOSClipboardVerdict(
                             /*source*/ google_url()));
  ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                         ->GetForBrowserContext(profile())
                         ->GetCopyRestrictedBySourceVerdict(
                             /*source*/ google_url()));
  EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                   ->GetForBrowserContext(profile())
                   ->BlockScreenshots(google_url()));
}

TEST_F(DataControlsRulesServiceAllFeaturesDisabledTest, NoVerdicts) {
  SetDataControls(profile()->GetPrefs(), {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "PRINTING", "level": "BLOCK"},
                      {"class": "CLIPBOARD", "level": "BLOCK"},
                      {"class": "SCREENSHOT", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPrintVerdict(google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPasteVerdict(
                          /*source*/ google_url_endpoint(),
                          /*destination*/ empty_endpoint(),
                          /*metadata*/ {}));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyToOSClipboardVerdict(
                          /*source*/ google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyRestrictedBySourceVerdict(
                          /*source*/ google_url()));
  EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                   ->GetForBrowserContext(profile())
                   ->BlockScreenshots(google_url()));
}

TEST_F(DataControlsRulesServiceTest, NoRuleSet) {
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPrintVerdict(google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetPasteVerdict(
                          /*source*/ google_url_endpoint(),
                          /*destination*/ empty_endpoint(),
                          /*metadata*/ {}));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyToOSClipboardVerdict(
                          /*source*/ google_url()));
  ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                      ->GetForBrowserContext(profile())
                      ->GetCopyRestrictedBySourceVerdict(
                          /*source*/ google_url()));
  EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                   ->GetForBrowserContext(profile())
                   ->BlockScreenshots(google_url()));
}

TEST_F(DataControlsRulesServiceTest, SourceURL) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"},
                        {"class": "PRINTING", "level": "BLOCK"},
                        {"class": "SCREENSHOT", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPrintVerdict(google_url()));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
    EXPECT_TRUE(ChromeRulesServiceFactory::GetInstance()
                    ->GetForBrowserContext(profile())
                    ->BlockScreenshots(google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "PRINTING", "level": "WARN"},
                        {"class": "SCREENSHOT", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPrintVerdict(google_url()));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ google_url_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetCopyToOSClipboardVerdict(
                              /*source*/ google_url()));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetCopyRestrictedBySourceVerdict(
                              /*source*/ google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->BlockScreenshots(google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "sources": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"},
                        {"class": "PRINTING", "level": "ALLOW"},
                        {"class": "SCREENSHOT", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "sources": {
                        "urls": ["https://*"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "PRINTING", "level": "WARN"},
                        {"class": "SCREENSHOT", "level": "BLOCK"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPrintVerdict(google_url()));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->BlockScreenshots(google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, DestinationURL) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});

    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});

    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ google_url_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "destinations": {
                        "urls": ["google.com"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "destinations": {
                        "urls": ["https://*"]
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, SourceIncognito) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"},
                        {"class": "SCREENSHOT", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ incognito_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(incognito_profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(incognito_profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    EXPECT_TRUE(ChromeRulesServiceFactory::GetInstance()
                    ->GetForBrowserContext(incognito_profile())
                    ->BlockScreenshots(google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->BlockScreenshots(google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "SCREENSHOT", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ incognito_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(incognito_profile())
                          ->GetCopyToOSClipboardVerdict(
                              /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(incognito_profile())
                          ->GetCopyRestrictedBySourceVerdict(
                              /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(incognito_profile())
                     ->BlockScreenshots(google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->BlockScreenshots(google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"},
                        {"class": "SCREENSHOT", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "sources": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"},
                        {"class": "SCREENSHOT", "level": "BLOCK"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ incognito_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ incognito_endpoint(),
                            /*metadata*/ {}));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(incognito_profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(incognito_profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(incognito_profile())
                     ->BlockScreenshots(google_url()));
    EXPECT_FALSE(ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(profile())
                     ->BlockScreenshots(google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, DestinationIncognito) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ incognito_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ incognito_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "destinations": {
                        "incognito": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ incognito_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ incognito_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, OSClipboardDestination) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetCopyToOSClipboardVerdict(
                              /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "destinations": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, NonOSClipboardDestination) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ google_url_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "destinations": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "destinations": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, SourceOtherProfile) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "sources": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ other_profile_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ other_profile_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "sources": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ other_profile_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ other_profile_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "sources": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "sources": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ other_profile_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ other_profile_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(incognito_profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, DestinationOtherProfile) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "destinations": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ other_profile_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ other_profile_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "destinations": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ other_profile_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ other_profile_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "destinations": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "destinations": {
                        "other_profile": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ other_profile_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ other_profile_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, OSClipboardSource) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ empty_endpoint(),
                              /*destination*/ google_url_endpoint(),
                              /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "sources": {
                        "os_clipboard": true
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ empty_endpoint(),
                               /*destination*/ google_url_endpoint(),
                               /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ google_url_endpoint(),
                            /*destination*/ empty_endpoint(),
                            /*metadata*/ {}));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }
}

TEST_F(DataControlsRulesServiceTest, NonOSClipboardSource) {
  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "block",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "BLOCK"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectBlockVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
  }

  {
    SetDataControls(profile()->GetPrefs(), {R"({
                      "name": "warn",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetPasteVerdict(
                              /*source*/ google_url_endpoint(),
                              /*destination*/ empty_endpoint(),
                              /*metadata*/ {}));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetCopyToOSClipboardVerdict(
                              /*source*/ google_url()));
    ExpectWarnVerdict(ChromeRulesServiceFactory::GetInstance()
                          ->GetForBrowserContext(profile())
                          ->GetCopyRestrictedBySourceVerdict(
                              /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(profile()->GetPrefs(), {
                                               R"({
                      "name": "allow",
                      "rule_id": "1234",
                      "sources": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "ALLOW"}
                      ]
                    })",
                                               R"({
                      "name": "warn",
                      "rule_id": "5678",
                      "sources": {
                        "os_clipboard": false
                      },
                      "restrictions": [
                        {"class": "CLIPBOARD", "level": "WARN"}
                      ]
                    })"});
    ExpectNoVerdict(ChromeRulesServiceFactory::GetInstance()
                        ->GetForBrowserContext(profile())
                        ->GetPasteVerdict(
                            /*source*/ empty_endpoint(),
                            /*destination*/ google_url_endpoint(),
                            /*metadata*/ {}));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetPasteVerdict(
                               /*source*/ google_url_endpoint(),
                               /*destination*/ empty_endpoint(),
                               /*metadata*/ {}));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyToOSClipboardVerdict(
                               /*source*/ google_url()));
    ExpectAllowVerdict(ChromeRulesServiceFactory::GetInstance()
                           ->GetForBrowserContext(profile())
                           ->GetCopyRestrictedBySourceVerdict(
                               /*source*/ google_url()));
  }
}

}  // namespace data_controls
