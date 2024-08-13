// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
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

  MOCK_METHOD(bool,
              GetFilteringBehaviorForURLWithAsyncChecks,
              (const GURL& url,
               FilteringBehaviorCallback callback,
               bool skip_manual_parent_filter));
};
}  // namespace

class ClassifyUrlNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      const GURL& initial_url,
      std::initializer_list<GURL> redirects = {}) {
    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            initial_url, main_rfh());

    // Note: this creates the throttle regardless the supervision status of the
    // user.
    std::unique_ptr<content::NavigationThrottle> throttle =
        ClassifyUrlNavigationThrottle::MakeUnique(navigation_handle_.get());

    // Add mock handlers for resume & cancel deferred.
    throttle->set_resume_callback_for_testing(
        base::BindLambdaForTesting([&]() { resume_called_ = true; }));
    return throttle;
  }

  SupervisedUserURLFilter* GetSupervisedUserURLFilter() {
    return SupervisedUserServiceFactory::GetForProfile(profile())
        ->GetURLFilter();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  bool resume_called() const { return resume_called_; }

 private:
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
  base::HistogramTester histogram_tester_;
  bool resume_called_ = false;
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
  ASSERT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());

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
  ASSERT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
}

TEST_F(ClassifyUrlNavigationThrottleTest,
       BlockedMatureSitesRecordedInBlockSafeSitesBucket) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [](const GURL& url,
             MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
             bool skip_manual_parent_filter) {
            std::move(callback).Run(FilteringBehavior::kBlock,
                                    FilteringBehaviorReason::ASYNC_CHECKER,
                                    /*is_uncertain=*/false);
            return true;
          });
  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
}

TEST_F(ClassifyUrlNavigationThrottleTest, ClassificationIsFasterThanHttp) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  std::vector<MockSupervisedUserURLFilter::FilteringBehaviorCallback> checks;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&checks](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            checks.push_back(std::move(callback));
            // Asynchronous behavior all the time.
            return false;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    testing::_, testing::_, testing::_))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL),
                               {GURL(kExampleURL), GURL(kExampleURL)});

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());

  // No checks are completed yet
  EXPECT_THAT(checks, testing::SizeIs(3));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // all checks
  for (auto& check : checks) {
    std::move(check).Run(FilteringBehavior::kAllow,
                         FilteringBehaviorReason::ASYNC_CHECKER,
                         /*is_uncertain=*/false);
  }

  // Throttle is not blocked
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // As a result, the navigation hadn't had to be resumed
  EXPECT_FALSE(resume_called());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 3);
  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*grew_by=*/1);
}

TEST_F(ClassifyUrlNavigationThrottleTest, ClassificationIsSlowerThanHttp) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  std::vector<MockSupervisedUserURLFilter::FilteringBehaviorCallback> checks;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&checks](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            checks.push_back(std::move(callback));
            // Asynchronous behavior all the time.
            return false;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    testing::_, testing::_, testing::_))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL),
                               {GURL(kExampleURL), GURL(kExampleURL)});

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());

  // At this point, no check was completed.
  EXPECT_THAT(checks, testing::SizeIs(3));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Complete two last checks
  std::move(checks[1]).Run(FilteringBehavior::kAllow,
                           FilteringBehaviorReason::ASYNC_CHECKER,
                           /*is_uncertain=*/false);
  std::move(checks[2]).Run(FilteringBehavior::kAllow,
                           FilteringBehaviorReason::ASYNC_CHECKER,
                           /*is_uncertain=*/false);

  // Now two out of three checks are complete
  EXPECT_THAT(checks, testing::SizeIs(3));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 2);

  // But will block at process response because one check is still
  // pending and no filtering was completed.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  // Now complete the outstanding check
  std::move(checks[0]).Run(FilteringBehavior::kAllow,
                           FilteringBehaviorReason::ASYNC_CHECKER,
                           /*is_uncertain=*/false);

  // As a result, the navigation is resumed (and three checks registered)
  EXPECT_TRUE(resume_called());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 3);
  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*grew_by=*/1);
}

TEST_F(ClassifyUrlNavigationThrottleTest, ShortCircuitsSynchronousBlock) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());

  std::vector<MockSupervisedUserURLFilter::FilteringBehaviorCallback> checks;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&checks](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            if (checks.empty()) {
              checks.push_back(std::move(callback));
              return false;
            }

            // Subsequent checks are synchronous blocks.
            std::move(callback).Run(FilteringBehavior::kBlock,
                                    FilteringBehaviorReason::ASYNC_CHECKER,
                                    /*is_uncertain=*/false);
            return true;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    testing::_, testing::_, testing::_))
      .Times(2);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL),
                               {GURL(kExampleURL), GURL(kExampleURL)});

  // It will DEFER at 2nd request (1st redirect).
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillRedirectRequest());

  // There will be one pending check
  EXPECT_THAT(checks, testing::SizeIs(1));
  // And one completed block from safe-sites (async checker)
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());

  // Since this is not a success path, no latency metric is recorded.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*grew_by=*/0);
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*grew_by=*/0);
}

TEST_F(ClassifyUrlNavigationThrottleTest, HandlesLateAsynchronousBlock) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());

  std::vector<MockSupervisedUserURLFilter::FilteringBehaviorCallback> checks;
  bool first_check_completed = false;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&checks, &first_check_completed](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            // First check is synchronous allow
            if (!first_check_completed) {
              first_check_completed = true;
              std::move(callback).Run(FilteringBehavior::kAllow,
                                      FilteringBehaviorReason::ASYNC_CHECKER,
                                      /*is_uncertain=*/false);
              return true;
            }
            // Subsequent checks are asynchronous
            checks.push_back(std::move(callback));
            return false;
          });

  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    testing::_, testing::_, testing::_))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL),
                               {GURL(kExampleURL), GURL(kExampleURL)});

  // It proceed all three request/redirects.
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());

  // There will be two pending checks (first was synchronous)
  EXPECT_THAT(checks, testing::SizeIs(2));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);

  // Http server completes first
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  // Complete first pending check
  std::move(checks.front())
      .Run(FilteringBehavior::kBlock, FilteringBehaviorReason::ASYNC_CHECKER,
           /*is_uncertain=*/false);

  // Now two out of three checks are complete
  EXPECT_THAT(checks, testing::SizeIs(2));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());

  // Since this is not a success path, no latency metric is recorded.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*grew_by=*/0);
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*grew_by=*/0);
}

}  // namespace supervised_user
