// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_refresh_navigation_throttle.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<MockAimEligibilityService>(
      *profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);
}

// This variation of the MockNavigationHandle only exists to easily mock
// situations where the handle doesn't represent the outermost frame since this
// is otherwise quite challenging to accomplish.
class MockNavigationHandleForOutermostFrame
    : public content::MockNavigationHandle {
 public:
  MockNavigationHandleForOutermostFrame(
      const GURL& url,
      content::RenderFrameHost* render_frame_host,
      bool is_outermost_frame,
      bool is_prerender_frame)
      : content::MockNavigationHandle(url, render_frame_host),
        is_outermost_(is_outermost_frame),
        is_prerender_(is_prerender_frame) {}
  ~MockNavigationHandleForOutermostFrame() override = default;
  bool IsInOutermostMainFrame() const override { return is_outermost_; }
  bool IsInPrerenderedMainFrame() const override { return is_prerender_; }

 private:
  bool is_outermost_;
  bool is_prerender_;
};

}  // namespace

class AimEligibilityRefreshNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AimEligibilityRefreshNavigationThrottleTest() = default;
  ~AimEligibilityRefreshNavigationThrottleTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndEnableFeature(omnibox::kAimUrlNavigationFetchEnabled);

    // Create TemplateURLService early.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    aim_eligibility_service_ = static_cast<MockAimEligibilityService*>(
        AimEligibilityServiceFactory::GetForProfile(profile()));
    ON_CALL(*aim_eligibility_service_, IsAimEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service_, HasAimUrlParams(testing::_))
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    aim_eligibility_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories;
    factories.emplace_back(
        AimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockAimEligibilityService));
    return factories;
  }

  content::NavigationThrottle::ThrottleAction RunThrottleForUrl(
      const GURL& url) {
    content::MockNavigationHandle handle(url, main_rfh());
    handle.set_is_in_primary_main_frame(true);

    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    AimEligibilityRefreshNavigationThrottle::MaybeCreateAndAdd(registry);

    CHECK_EQ(1u, registry.throttles().size());
    auto* throttle = static_cast<AimEligibilityRefreshNavigationThrottle*>(
        registry.throttles().back().get());
    return throttle->WillStartRequest().action();
  }

  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockAimEligibilityService> aim_eligibility_service_ = nullptr;
};

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       NotCreatedForNonOutermostMainFrame) {
  const GURL url("https://www.google.com/search?udm=50&q=query");
  MockNavigationHandleForOutermostFrame handle(url, main_rfh(),
                                               /*is_outermost_frame=*/false,
                                               /*is_prerender_frame=*/false);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  AimEligibilityRefreshNavigationThrottle::MaybeCreateAndAdd(registry);

  EXPECT_TRUE(registry.throttles().empty());
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       NotCreatedForPrerenderFrame) {
  const GURL url("https://www.google.com/search?udm=50&q=query");
  MockNavigationHandleForOutermostFrame handle(url, main_rfh(),
                                               /*is_outermost_frame=*/true,
                                               /*is_prerender_frame=*/true);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  AimEligibilityRefreshNavigationThrottle::MaybeCreateAndAdd(registry);

  EXPECT_TRUE(registry.throttles().empty());
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest, NoFetchForNonHTTPUrl) {
  const GURL url("chrome://flags/");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       NoFetchForNonGoogleSearchUrl) {
  const GURL url("https://example.com/");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       NoFetchForNonGoogleSearchUrlWithSearchQuery) {
  const GURL url("https://example.com/search?udm=50");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       FetchForGoogleSearchUrlNoQuery) {
  const GURL url("https://www.google.com/search?udm=50");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(1);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       FetchForGoogleSearchUrlEmptyQuery) {
  const GURL url("https://www.google.com/search?udm=50&q=");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(1);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       FetchForGoogleSearchUrlNoQueryInternational) {
  const GURL url("https://www.google.co.uk/search?udm=50");

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(1);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest,
       FetchEligibilityOnAimSearchUrl) {
  const GURL url("https://www.google.com/search?udm=50&q=query");

  EXPECT_CALL(
      *aim_eligibility_service_,
      FetchEligibility(AimEligibilityService::RequestSource::kAimUrlNavigation))
      .Times(1);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest, NoFetchWhenIneligible) {
  const GURL url("https://www.google.com/search?udm=50&q=query");

  ON_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}

TEST_F(AimEligibilityRefreshNavigationThrottleTest, NoFetchWithoutAimParams) {
  const GURL url("https://www.google.com/search?udm=50&q=query");

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(testing::_))
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  EXPECT_EQ(content::NavigationThrottle::PROCEED, RunThrottleForUrl(url));
}
