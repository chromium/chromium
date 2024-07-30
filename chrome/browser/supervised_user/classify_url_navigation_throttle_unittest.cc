// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {

static const char* kExampleURL = "https://example.com/";

class MockSupervisedUserURLFilter : public SupervisedUserURLFilter {
 public:
  explicit MockSupervisedUserURLFilter(PrefService& prefs)
      : SupervisedUserURLFilter(
            prefs,
            std::make_unique<safe_search_api::FakeURLCheckerClient>(),
            base::BindRepeating([](const GURL& url) { return false; })) {}

  MOCK_METHOD(FilteringBehavior,
              GetFilteringBehaviorForURL,
              (const GURL& url, FilteringBehaviorReason* reason),
              (override));
};
}  // namespace

class ClassifyUrlNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            url, main_rfh());

    // Note: this creates the throttle regardless the supervision status of the
    // user.
    return ClassifyUrlNavigationThrottle::MakeUnique(navigation_handle_.get());
  }

  SupervisedUserURLFilter* GetSupervisedUserURLFilter() {
    return SupervisedUserServiceFactory::GetForProfile(profile())
        ->GetURLFilter();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ClassifyUrlNavigationThrottleTest, AllowedUrlsRecordedInAllowBucket) {
  GURL allowed_url(kExampleURL);
  std::map<std::string, bool> hosts{{allowed_url.host(), true}};
  GetSupervisedUserURLFilter()->SetManualHosts(std::move(hosts));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(allowed_url);
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*expected_count(grew by)*/ 1);
}

TEST_F(ClassifyUrlNavigationThrottleTest,
       BlocklistedUrlsRecordedInBlockManualBucket) {
  GURL blocked_url(kExampleURL);
  std::map<std::string, bool> hosts;
  hosts[blocked_url.host()] = false;
  GetSupervisedUserURLFilter()->SetManualHosts(std::move(hosts));
  ASSERT_EQ(
      FilteringBehavior::kBlock,
      GetSupervisedUserURLFilter()->GetFilteringBehaviorForURL(blocked_url));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(blocked_url);
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);
}

TEST_F(ClassifyUrlNavigationThrottleTest,
       AllSitesBlockedRecordedInBlockNotInAllowlistBucket) {
  GetSupervisedUserURLFilter()->SetDefaultFilteringBehavior(
      FilteringBehavior::kBlock);

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
}

TEST_F(ClassifyUrlNavigationThrottleTest,
       BlockedMatureSitesRecordedInBlockSafeSitesBucket) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURL(testing::_, testing::_))
      .WillByDefault([](const GURL& url, FilteringBehaviorReason* reason) {
        *reason = FilteringBehaviorReason::ASYNC_CHECKER;
        return FilteringBehavior::kBlock;
      });
  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
}

}  // namespace supervised_user
