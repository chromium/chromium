// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
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
      : SupervisedUserURLFilter(prefs,
                                std::make_unique<FakeURLFilterDelegate>()) {}

  MOCK_METHOD(SupervisedUserURLFilter::Result,
              GetFilteringBehavior,
              (const GURL& url));
};
}  // namespace

class SupervisedUserNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetIsSupervisedProfile();
    ASSERT_TRUE(profile()->IsChild());
  }

  std::unique_ptr<SupervisedUserNavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            url, main_rfh());
    return SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(
        navigation_handle_.get());
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

TEST_F(SupervisedUserNavigationThrottleTest, AllowedUrlsRecordedInAllowBucket) {
  GURL allowed_url(kExampleURL);
  std::map<std::string, bool> hosts{{allowed_url.host(), true}};
  GetSupervisedUserURLFilter()->SetManualHosts(std::move(hosts));
  CreateNavigationThrottle(allowed_url)->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       BlocklistedUrlsRecordedInBlockManualBucket) {
  GURL blocked_url(kExampleURL);
  std::map<std::string, bool> hosts;
  hosts[blocked_url.host()] = false;
  GetSupervisedUserURLFilter()->SetManualHosts(std::move(hosts));
  ASSERT_TRUE(GetSupervisedUserURLFilter()
                  ->GetFilteringBehavior(blocked_url)
                  .IsBlocked());

  CreateNavigationThrottle(blocked_url)->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       AllSitesBlockedRecordedInBlockNotInAllowlistBucket) {
  GetSupervisedUserURLFilter()->SetDefaultFilteringBehavior(
      FilteringBehavior::kBlock);

  CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       BlockedMatureSitesRecordedInBlockSafeSitesBucket) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  ON_CALL(*mock_url_filter, GetFilteringBehavior(testing::_))
      .WillByDefault([](const GURL& url) -> SupervisedUserURLFilter::Result {
        return {url, FilteringBehavior::kBlock,
                FilteringBehaviorReason::ASYNC_CHECKER};
      });
  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
}

}  // namespace supervised_user
