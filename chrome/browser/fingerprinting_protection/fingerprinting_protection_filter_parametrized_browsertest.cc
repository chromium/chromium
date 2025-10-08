// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/common/throttle_creation_result.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#else
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace fingerprinting_protection_filter {
namespace {

constexpr const char kFooHostName[] = "myhost.test";
constexpr const char kBarHostName[] = "myotherhost.test";

constexpr const char kRendererThrottleCreationResultMetricName[] =
    "FingerprintingProtection.RendererThrottleCreationResult";

enum class FeatureConfig {
  // The feature flags are not changed from their default values.
  kDefault,
  // The feature is enabled in regular browsing in dry run mode.
  kDryRun,
  // The feature is enabled in regular browsing.
  kEnabledInRegular,
  // The feature is enabled in incognito browsing.
  kEnabledInIncognito,
  // The feature is enabled in both regular and incognito browsing.
  kEnabledInBoth,
  // The feature is disabled in both regular and incognito browsing.
  kDisabled,
};

enum class BrowserMode { kRegular, kIncognito };

// Whether the feature is enabled at all for a given browser mode. This is
// different from `ShouldBlock` because the feature may be enabled but not
// block any requests - for example, if it is in dry run mode.
using FeatureEnabledForBrowserMode =
    base::StrongAlias<class FeatureEnabledTag, bool>;
// Whether the feature should block requests that match the ruleset in a given
// browser mode.
using ShouldBlock = base::StrongAlias<class ShouldBlockTag, bool>;

struct TestConfiguration {
  FeatureConfig feature_config;
  BrowserMode browser_mode;
  FeatureEnabledForBrowserMode feature_enabled_for_browser_mode;
  ShouldBlock should_block;
};

// A browsertest that checks for fingerprinting protection filter behavior. This
// test suite is parameterized to run all of its tests under various
// configurations defined by the `TestConfiguration` struct.
//
// The `kTestConfigurations` array defines the full set of configurations. Each
// test case (defined using `IN_PROC_BROWSER_TEST_P`) will be executed for each
// configuration in this array. This allows developers to write a single test
// implementation and have it automatically verified across different feature
// states and browser modes.
//
// To add a new test, simply implement a new `IN_PROC_BROWSER_TEST_P` test body.
class FingerprintingProtectionFilterParametrizedBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<TestConfiguration> {
 public:
  FingerprintingProtectionFilterParametrizedBrowserTest() {
    const TestConfiguration& config = GetParam();
    switch (config.feature_config) {
      case FeatureConfig::kDefault:
        break;
      case FeatureConfig::kDryRun:
        scoped_feature_list_.InitWithFeaturesAndParameters(
            /*enabled_features=*/
            {{features::kEnableFingerprintingProtectionFilter,
              {{"activation_level", "dry_run"},
               {"performance_measurement_rate", "1.0"}}}},
            /*disabled_features=*/{
                {features::kEnableFingerprintingProtectionFilterInIncognito},
                {privacy_sandbox::kFingerprintingProtectionUx}});
        break;
      case FeatureConfig::kEnabledInRegular:
        scoped_feature_list_.InitWithFeaturesAndParameters(
            /*enabled_features=*/
            {{features::kEnableFingerprintingProtectionFilter,
              {{"performance_measurement_rate", "1.0"}}}},
            /*disabled_features=*/{
                {features::kEnableFingerprintingProtectionFilterInIncognito},
                {privacy_sandbox::kFingerprintingProtectionUx}});
        break;
      case FeatureConfig::kEnabledInIncognito:
        scoped_feature_list_.InitWithFeaturesAndParameters(
            /*enabled_features=*/
            {{features::kEnableFingerprintingProtectionFilterInIncognito,
              {{"performance_measurement_rate", "1.0"}}},
             {privacy_sandbox::kFingerprintingProtectionUx, {}}},
            /*disabled_features=*/{
                {features::kEnableFingerprintingProtectionFilter}});
        break;
      case FeatureConfig::kEnabledInBoth:
        scoped_feature_list_.InitWithFeaturesAndParameters(
            /*enabled_features=*/
            {{features::kEnableFingerprintingProtectionFilter,
              {{"performance_measurement_rate", "1.0"}}},
             {features::kEnableFingerprintingProtectionFilterInIncognito,
              {{"performance_measurement_rate", "1.0"}}},
             {privacy_sandbox::kFingerprintingProtectionUx, {}}},
            /*disabled_features=*/{});
        break;
      case FeatureConfig::kDisabled:
        scoped_feature_list_.InitWithFeatureStates(
            {{features::kEnableFingerprintingProtectionFilterInIncognito,
              false},
             {features::kEnableFingerprintingProtectionFilter, false},
             {privacy_sandbox::kFingerprintingProtectionUx, false}});
        break;
    }
  }

  FingerprintingProtectionFilterParametrizedBrowserTest(
      const FingerprintingProtectionFilterParametrizedBrowserTest&) = delete;
  FingerprintingProtectionFilterParametrizedBrowserTest& operator=(
      const FingerprintingProtectionFilterParametrizedBrowserTest&) = delete;

  ~FingerprintingProtectionFilterParametrizedBrowserTest() override = default;

 protected:
  // Returns a URL for a test frame which will load a script from the given
  // URL, to allow testing both same-site and cross-site scripts .
  GURL GetUrlForFrameWithScriptParam(const GURL& frame_url,
                                     const GURL& script_url) const {
    return net::AppendQueryParameter(frame_url, "script_url",
                                     script_url.spec());
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/fingerprinting_protection");
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Switch to incognito mode in a platform-specific way if needed.
    if (GetParam().browser_mode == BrowserMode::kIncognito) {
#if BUILDFLAG(IS_ANDROID)
      Profile* incognito_profile =
          chrome_test_utils::GetProfile(this)->GetPrimaryOTRProfile(
              /*create_if_needed=*/true);

      // ActivityType that doesn't restore tabs on cold start. Any type other
      // than kTabbed is fine.
      const auto kTestActivityType = chrome::android::ActivityType::kCustomTab;
      incognito_tab_model_ = std::make_unique<OwningTestTabModel>(
          incognito_profile, kTestActivityType);
      incognito_tab_model_->SetIsActiveModel(true);
      incognito_tab_model_->AddEmptyTab(0, /*select=*/true);
      content::WebContents* web_contents =
          incognito_tab_model_->GetActiveWebContents();
      TabAndroid::AttachTabHelpers(web_contents);
      // Navigate to about:blank so it behaves the same as a new tab on desktop.
      ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("about:blank")));
#else
      Browser* incognito = CreateIncognitoBrowser(browser()->profile());
      CloseBrowserSynchronously(browser());
      SelectFirstBrowser();
      ASSERT_EQ(browser(), incognito);
#endif  // BUILDFLAG(IS_ANDROID)
    }
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_ANDROID)
    incognito_tab_model_.reset();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void SetRulesetToDisallowURLsWithSubstring(const std::string& substring) {
    // The ruleset service is null when fingerprinting protection is disabled.
    if (!g_browser_process->fingerprinting_protection_ruleset_service()) {
      return;
    }

    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    ruleset_creator_.CreateRulesetToDisallowURLWithSubstrings(
        {substring}, &test_ruleset_pair);

    subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
        g_browser_process->fingerprinting_protection_ruleset_service());
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
  }

  bool WasScriptBlocked(const content::ToRenderFrameHost& rfh) const {
    // TODO(crbug.com/445469491): Consider having scripts add to a list of
    // executed scrips and checking that list here instead.
    return !content::EvalJs(rfh.render_frame_host(),
                            "!!document.scriptExecuted")
                .ExtractBool();
  }

  void CheckSubresourceMetrics(const base::HistogramTester& histogram_tester,
                               RendererThrottleCreationResult expected_result) {
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    if (IsFeatureEnabledForBrowserMode()) {
      EXPECT_THAT(histogram_tester.GetAllSamples(
                      kRendererThrottleCreationResultMetricName),
                  base::BucketsAre(base::Bucket(expected_result, 1)));
    } else {
      histogram_tester.ExpectTotalCount(
          kRendererThrottleCreationResultMetricName, 0);
    }
  }

 protected:
  content::WebContents* GetWebContents() const {
#if BUILDFLAG(IS_ANDROID)
    if (GetParam().browser_mode == BrowserMode::kIncognito) {
      return incognito_tab_model_->GetActiveWebContents();
    }
#endif  // BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool ShouldBlockRequests() const { return GetParam().should_block.value(); }

  bool IsFeatureEnabledForBrowserMode() const {
    return GetParam().feature_enabled_for_browser_mode.value();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_ANDROID)
  // Unique pointer to the incognito tab model to keep it alive for the
  // duration of each test.
  std::unique_ptr<OwningTestTabModel> incognito_tab_model_;
#endif

  subresource_filter::testing::TestRulesetCreator ruleset_creator_;
};

constexpr const TestConfiguration kTestConfigurations[] = {
    // kDefault
    // This will use the feature flag state from
    // `testing/variations/fieldtrial_testing_config.json` and doesn't
    // necessarily match the default feature flag state that end users will see.
    // Currently this means dry run mode is enabled for regular browsing and
    // blocking is enabled for incognito.
    {FeatureConfig::kDefault, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(true), ShouldBlock(false)},
    {FeatureConfig::kDefault, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(true), ShouldBlock(true)},
    // kDryRun
    {FeatureConfig::kDryRun, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(true), ShouldBlock(false)},
    {FeatureConfig::kDryRun, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(false), ShouldBlock(false)},
    // kEnabledInRegular
    {FeatureConfig::kEnabledInRegular, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(true), ShouldBlock(true)},
    {FeatureConfig::kEnabledInRegular, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(false), ShouldBlock(false)},
    // kEnabledInIncognito
    {FeatureConfig::kEnabledInIncognito, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(false), ShouldBlock(false)},
    {FeatureConfig::kEnabledInIncognito, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(true), ShouldBlock(true)},
    // kEnabledInBoth
    {FeatureConfig::kEnabledInBoth, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(true), ShouldBlock(true)},
    {FeatureConfig::kEnabledInBoth, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(true), ShouldBlock(true)},
    // kDisabled
    {FeatureConfig::kDisabled, BrowserMode::kRegular,
     FeatureEnabledForBrowserMode(false), ShouldBlock(false)},
    {FeatureConfig::kDisabled, BrowserMode::kIncognito,
     FeatureEnabledForBrowserMode(false), ShouldBlock(false)},
};

// 1. Main frame navigations

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_SameSiteNavigationsAllowed) {
  GURL test_url = embedded_test_server()->GetURL(
      kFooHostName, "/frame_with_included_script.html");

  // Check that a same-site main frame navigation is never blocked since these
  // are generally the result of some user interaction.
  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");

  // The first navigation will be from about:blank to the test url.
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));

  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));

  // The second navigation is same-site since it again visits the test url.
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));
  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));
}

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_CrossSiteNavigationsAllowed) {
  GURL test_url = embedded_test_server()->GetURL(
      kFooHostName, "/frame_with_included_script.html");
  GURL cross_site_test_url = embedded_test_server()->GetURL(
      kBarHostName, "/frame_with_included_script.html");

  // Check that a cross-site main frame navigation is never blocked since these
  // are generally the result of some user interaction.
  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");

  // Navigate to the first test url, then to the cross-site test url and
  // make sure the script on the frame was allowed to load.
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), cross_site_test_url));
  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));
}

// 2. Main frame subresources

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_SameSiteSubresourcesAllowed) {
  base::HistogramTester histogram_tester;
  GURL test_url = embedded_test_server()->GetURL(
      kFooHostName, "/frame_with_included_script.html");

  // Check that a same-site subresource is not blocked even if it matches a
  // rule.
  SetRulesetToDisallowURLsWithSubstring("included_script.js");

  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));

  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));

  CheckSubresourceMetrics(histogram_tester,
                          RendererThrottleCreationResult::kSkipSameSite);
}

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_CrossSiteSubresourcesAllowed) {
  base::HistogramTester histogram_tester;
  GURL test_url = GetUrlForFrameWithScriptParam(
      embedded_test_server()->GetURL(kFooHostName,
                                     "/frame_with_included_script.html"),
      embedded_test_server()->GetURL(kBarHostName, "/included_script.js"));

  // Check that when the feature is enabled, throttles are created but the
  // script is allowed when no rules match.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));

  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));
  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));

  CheckSubresourceMetrics(histogram_tester,
                          RendererThrottleCreationResult::kCreate);
}

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_CrossSiteSubresourcesBlocked) {
  base::HistogramTester histogram_tester;
  GURL test_url = GetUrlForFrameWithScriptParam(
      embedded_test_server()->GetURL(kFooHostName,
                                     "/frame_with_included_script.html"),
      embedded_test_server()->GetURL(kBarHostName, "/included_script.js"));

  // Check that the script is blocked when the feature is enabled and a
  // rule matches.
  SetRulesetToDisallowURLsWithSubstring("included_script.js");

  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));

  EXPECT_EQ(ShouldBlockRequests(), WasScriptBlocked(GetWebContents()));

  CheckSubresourceMetrics(histogram_tester,
                          RendererThrottleCreationResult::kCreate);
}

IN_PROC_BROWSER_TEST_P(FingerprintingProtectionFilterParametrizedBrowserTest,
                       MainFrame_CrossSiteSubresourcesOnLocalHostAllowed) {
  base::HistogramTester histogram_tester;
  // The frame is non-localhost, but calling `embedded_test_server()->GetURL()`
  // without a host means the script is localhost and thus also cross-site with
  // respect to the frame.
  GURL test_url = GetUrlForFrameWithScriptParam(
      embedded_test_server()->GetURL(kFooHostName,
                                     "/frame_with_included_script.html"),
      embedded_test_server()->GetURL("/included_script.js"));

  // Check that the script is not blocked when the feature is enabled even
  // though it matches a rule.
  SetRulesetToDisallowURLsWithSubstring("included_script.js");

  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), test_url));
  EXPECT_FALSE(WasScriptBlocked(GetWebContents()));

  CheckSubresourceMetrics(histogram_tester,
                          RendererThrottleCreationResult::kSkipLocalHost);
}

// TODO(https://crbug.com/445469491): Add tests for subframe navigation and
// subresource blocking, navigation and subresource redirects, cancelled
// navigations (downloads), redirect heuristic exceptions, privacy settings, and
// UMA metrics.

INSTANTIATE_TEST_SUITE_P(
    All,
    FingerprintingProtectionFilterParametrizedBrowserTest,
    testing::ValuesIn(kTestConfigurations),
    [](const testing::TestParamInfo<TestConfiguration>& info) {
      std::string name;
      switch (info.param.feature_config) {
        case FeatureConfig::kDefault:
          name += "Default";
          break;
        case FeatureConfig::kDryRun:
          name += "DryRun";
          break;
        case FeatureConfig::kEnabledInRegular:
          name += "EnabledInRegular";
          break;
        case FeatureConfig::kEnabledInIncognito:
          name += "EnabledInIncognito";
          break;
        case FeatureConfig::kEnabledInBoth:
          name += "EnabledInBoth";
          break;
        case FeatureConfig::kDisabled:
          name += "Disabled";
          break;
      }
      switch (info.param.browser_mode) {
        case BrowserMode::kRegular:
          name += "_Regular";
          break;
        case BrowserMode::kIncognito:
          name += "_Incognito";
          break;
      }
      return name;
    });

}  // namespace
}  // namespace fingerprinting_protection_filter
