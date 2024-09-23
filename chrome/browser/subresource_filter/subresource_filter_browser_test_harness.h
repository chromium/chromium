// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace proto = url_pattern_index::proto;

using subresource_filter::testing::ScopedSubresourceFilterConfigurator;
using subresource_filter::testing::TestRulesetPublisher;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

class TestSafeBrowsingDatabaseHelper;

namespace subresource_filter {

class AdsInterventionManager;
class SubresourceFilterContentSettingsManager;
class RulesetService;

// Small helper mock to allow tests to set expectations about observations on
// the subresource filter. Automatically registers and unregisters itself as an
// observer for its lifetime.
class MockSubresourceFilterObserver : public SubresourceFilterObserver {
 public:
  explicit MockSubresourceFilterObserver(content::WebContents* web_contents);
  ~MockSubresourceFilterObserver() override;

  MOCK_METHOD2(OnPageActivationComputed,
               void(content::NavigationHandle*, const mojom::ActivationState&));

 private:
  base::ScopedObservation<SubresourceFilterObserverManager,
                          SubresourceFilterObserver>
      scoped_observation_{this};
};

// Matchers for mojom::ActivationState arguments.
MATCHER(HasActivationLevelDisabled, "") {
  return arg.activation_level == mojom::ActivationLevel::kDisabled;
}
MATCHER(HasActivationLevelDryRun, "") {
  return arg.activation_level == mojom::ActivationLevel::kDryRun;
}
MATCHER(HasActivationLevelEnabled, "") {
  return arg.activation_level == mojom::ActivationLevel::kEnabled;
}

// Class for shared test harness functionality to be used for both safe browsing
// and fingerprinting protection browser tests.
class SubresourceFilterSharedBrowserTest : public PlatformBrowserTest {
 public:
  SubresourceFilterSharedBrowserTest();

  SubresourceFilterSharedBrowserTest(
      const SubresourceFilterSharedBrowserTest&) = delete;
  SubresourceFilterSharedBrowserTest& operator=(
      const SubresourceFilterSharedBrowserTest&) = delete;

  ~SubresourceFilterSharedBrowserTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  GURL GetTestUrl(const std::string& relative_url) const;

  content::WebContents* web_contents();

  content::RenderFrameHost* FindFrameByName(const std::string& name);

  bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh);

  void ExpectParsedScriptElementLoadedStatusInFrames(
      const std::vector<const char*>& frame_names,
      const std::vector<bool>& expect_loaded);

  void ExpectFramesIncludedInLayout(const std::vector<const char*>& frame_names,
                                    const std::vector<bool>& expect_displayed);

  void NavigateFrame(const char* frame_name, const GURL& url);
};

class SubresourceFilterBrowserTest : public SubresourceFilterSharedBrowserTest {
 public:
  SubresourceFilterBrowserTest();

  SubresourceFilterBrowserTest(const SubresourceFilterBrowserTest&) = delete;
  SubresourceFilterBrowserTest& operator=(const SubresourceFilterBrowserTest&) =
      delete;

  ~SubresourceFilterBrowserTest() override;

  // Names of DocumentLoad histograms.

  static constexpr const char kSubresourceLoadsTotalForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Total";
  static constexpr const char kSubresourceLoadsEvaluatedForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated";
  static constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules";
  static constexpr const char kSubresourceLoadsDisallowedForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed";

  // Names of the performance measurement histograms.
  static constexpr const char kEvaluationTotalWallDurationForPage[] =
      "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration";
  static constexpr const char kEvaluationTotalCPUDurationForPage[] =
      "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration";
  static constexpr const char kEvaluationWallDuration[] =
      "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration";
  static constexpr const char kEvaluationCPUDuration[] =
      "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration";

  static constexpr const char kActivationDecision[] =
      "SubresourceFilter.PageLoad.ActivationDecision";

  // Names of navigation chain patterns histogram.
  static constexpr const char kActivationListHistogram[] =
      "SubresourceFilter.PageLoad.ActivationList";

  static constexpr const char kPageLoadActivationStateHistogram[] =
      "SubresourceFilter.PageLoad.ActivationState";
  static constexpr const char kPageLoadActivationStateDidInheritHistogram[] =
      "SubresourceFilter.PageLoad.ActivationState.DidInherit";

  // Other histograms.
  static constexpr const char kSubresourceFilterActionsHistogram[] =
      "SubresourceFilter.Actions2";

  bool AdsBlockedInContentSettings(content::RenderFrameHost* frame_host);

 protected:
  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;

  virtual std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase();

  void ConfigureAsPhishingURL(const GURL& url);

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url);

  void ConfigureURLWithWarning(
      const GURL& url,
      std::vector<safe_browsing::SubresourceFilterType> filter_types);

  SubresourceFilterContentSettingsManager* settings_manager() const {
    return profile_context_->settings_manager();
  }

  AdsInterventionManager* ads_intervention_manager() {
    return profile_context_->ads_intervention_manager();
  }

  bool IsDynamicScriptElementLoaded(content::RenderFrameHost* rfh);

  void InsertDynamicFrameWithScript();

  void NavigateFromRendererSide(const GURL& url);

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetToDisallowURLsWithSubstrings(
      std::vector<std::string_view> substrings);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  // Re-initializes the ruleset_service by opening the ruleset file provided
  // by indexed_ruleset_path and publishing it.
  void OpenAndPublishRuleset(RulesetService* ruleset_service,
                             const base::FilePath& indexed_ruleset_path);

  void ResetConfiguration(Configuration config);

  void ResetConfigurationToEnableOnPhishingSites(
      bool measure_performance = false);

  TestSafeBrowsingDatabaseHelper* database_helper() {
    return database_helper_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  TestRulesetCreator ruleset_creator_;
  ScopedSubresourceFilterConfigurator scoped_configuration_;

  std::unique_ptr<TestSafeBrowsingDatabaseHelper> database_helper_;

  // Owned by the profile.
  raw_ptr<SubresourceFilterProfileContext, AcrossTasksDanglingUntriaged>
      profile_context_;
};

// This class automatically syncs the SubresourceFilter SafeBrowsing list
// without needing a chrome branded build.
class SubresourceFilterListInsertingBrowserTest
    : public SubresourceFilterBrowserTest {
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase() override;
};

class SubresourceFilterPrerenderingBrowserTest
    : public SubresourceFilterListInsertingBrowserTest {
 public:
  SubresourceFilterPrerenderingBrowserTest();
  ~SubresourceFilterPrerenderingBrowserTest() override;

  void SetUp() override;

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

class SubresourceFilterFencedFrameBrowserTest
    : public SubresourceFilterListInsertingBrowserTest {
 public:
  SubresourceFilterFencedFrameBrowserTest() = default;
  ~SubresourceFilterFencedFrameBrowserTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
