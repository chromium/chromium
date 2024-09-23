// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/safe_browsing_child_navigation_throttle.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

using content_settings::CookieSettings;

const char kChildFrameNavigationDeferredForAdTaggingHistogram[] =
    "PageLoad.FrameCounts.AdFrames.PerFrame.DeferredForTagging";

}  // namespace

class SafeBrowsingChildNavigationThrottleAdTaggingTest
    : public ChildFrameNavigationFilteringThrottleTestHarness {
 public:
  SafeBrowsingChildNavigationThrottleAdTaggingTest() = default;

  SafeBrowsingChildNavigationThrottleAdTaggingTest(
      const SafeBrowsingChildNavigationThrottleAdTaggingTest&) = delete;
  SafeBrowsingChildNavigationThrottleAdTaggingTest& operator=(
      const SafeBrowsingChildNavigationThrottleAdTaggingTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kTPCDAdHeuristicSubframeRequestTagging,
          {{"check_exceptions", "true"}}},
         {net::features::kTopLevelTpcdTrialSettings, {}},
         {net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    ChildFrameNavigationFilteringThrottleTestHarness::SetUp();
    settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(browser_context());
    profile_interaction_manager_ = std::make_unique<ProfileInteractionManager>(
        SubresourceFilterProfileContextFactory::GetForProfile(profile()));
  }

  // content::RenderViewHostTestHarness:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  Profile* profile() { return Profile::FromBrowserContext(browser_context()); }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
    // The |parent_filter_| is the parent frame's filter. Do not register a
    // throttle if the parent is not activated with a valid filter.
    if (parent_filter_) {
      auto throttle = std::make_unique<SafeBrowsingChildNavigationThrottle>(
          navigation_handle, parent_filter_.get(),
          profile_interaction_manager_->AsWeakPtr(),
          base::BindRepeating([](const GURL& filtered_url) {
            return base::StringPrintf(
                kDisallowChildFrameConsoleMessageFormat,
                filtered_url.possibly_invalid_spec().c_str());
          }),
          /*ad_evidence=*/std::nullopt);
      ASSERT_NE(nullptr, throttle->GetNameForLogging());
      navigation_handle->RegisterThrottleForTesting(std::move(throttle));
    }
  }

  void CreateSubframeAndInitNavigation(const GURL& first_url,
                                       content::RenderFrameHost* parent) {
    content::RenderFrameHost* render_frame =
        content::RenderFrameHostTester::For(parent)->AppendChild(
            base::StringPrintf("subframe-%s", first_url.spec().c_str()));
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              render_frame);
    navigation_simulator_->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    navigation_simulator_->SetAutoAdvance(false);
  }

  void Create3pTrialSetting(GURL first_party_url, GURL third_party_url) {
    settings_map_->SetContentSettingDefaultScope(
        third_party_url, first_party_url, ContentSettingsType::TPCD_TRIAL,
        CONTENT_SETTING_ALLOW);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<HostContentSettingsMap> settings_map_;
  std::unique_ptr<ProfileInteractionManager> profile_interaction_manager_;
};

TEST_F(SafeBrowsingChildNavigationThrottleAdTaggingTest,
       FrameNavigationNotDeferred) {
  base::HistogramTester histogram_tester;

  InitializeDocumentSubresourceFilter(
      GURL("https://example.test"),
      subresource_filter::mojom::ActivationLevel::kDryRun);

  CreateSubframeAndInitNavigation(GURL("https://child-frame.test"), main_rfh());
  navigation_simulator()->Start();

  // The navigation should NOT be deferred, since no 3P cookie exception
  // applies.
  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator()->GetLastThrottleCheckResult().action());

  histogram_tester.ExpectUniqueSample(
      kChildFrameNavigationDeferredForAdTaggingHistogram, false, 1);
}

TEST_F(SafeBrowsingChildNavigationThrottleAdTaggingTest,
       FrameNavigationDeferredIfExceptionMatch) {
  base::HistogramTester histogram_tester;
  Create3pTrialSetting(GURL("https://example.test"),
                       GURL("https://excepted-child-frame.test"));

  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  CreateSubframeAndInitNavigation(GURL("https://excepted-child-frame.test"),
                                  main_rfh());
  navigation_simulator()->Start();

  // The navigation should be deferred, since a 3P cookie exception applies.
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  histogram_tester.ExpectUniqueSample(
      kChildFrameNavigationDeferredForAdTaggingHistogram, true, 1);
}

TEST_F(SafeBrowsingChildNavigationThrottleAdTaggingTest,
       FrameRedirectNavigationDeferredIfExceptionMatch) {
  Create3pTrialSetting(GURL("https://example.test"),
                       GURL("https://excepted-child-frame.test"));

  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  CreateSubframeAndInitNavigation(GURL("https://child-frame.test"), main_rfh());

  base::HistogramTester histogram_tester;

  // The navigation should NOT be deferred, since no 3P cookie exception
  // applies.
  navigation_simulator()->Start();
  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator()->GetLastThrottleCheckResult().action());

  histogram_tester.ExpectUniqueSample(
      kChildFrameNavigationDeferredForAdTaggingHistogram, false, 1);

  // Redirect the navigation to a URL that a 3P cookie exception applies to
  // (under the current top-level origin).
  navigation_simulator()->Redirect(GURL("https://excepted-child-frame.test"));

  // The navigation should be deferred.
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  histogram_tester.ExpectTotalCount(
      kChildFrameNavigationDeferredForAdTaggingHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kChildFrameNavigationDeferredForAdTaggingHistogram, true,
      1);  // from excepted-child-frame.test
}

class SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest
    : public SafeBrowsingChildNavigationThrottleAdTaggingTest {
 public:
  SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest() = default;

  SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest(
      const SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest&) =
      delete;
  SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest& operator=(
      const SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest&) =
      delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kTPCDAdHeuristicSubframeRequestTagging,
          {{"check_exceptions", "false"}}},
         {net::features::kTopLevelTpcdTrialSettings, {}},
         {net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    ChildFrameNavigationFilteringThrottleTestHarness::SetUp();
    settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(browser_context());
    profile_interaction_manager_ = std::make_unique<ProfileInteractionManager>(
        SubresourceFilterProfileContextFactory::GetForProfile(profile()));
  }
};

TEST_F(SafeBrowsingChildNavigationThrottleExceptionCheckDisabledTest,
       FrameNavigationDeferredWithoutException) {
  base::HistogramTester histogram_tester;

  InitializeDocumentSubresourceFilter(
      GURL("https://example.test"),
      subresource_filter::mojom::ActivationLevel::kDryRun);

  CreateSubframeAndInitNavigation(GURL("https://child-frame.test"), main_rfh());
  navigation_simulator()->Start();

  // The navigation should be deferred, even though no 3P cookie exception
  // applies, since exception checking is disabled.
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  histogram_tester.ExpectUniqueSample(
      kChildFrameNavigationDeferredForAdTaggingHistogram, true, 1);
}

}  // namespace subresource_filter
