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
static const char* kExample1URL = "https://example1.com/";
static const char* kExample2URL = "https://example2.com/";

void ExpectThrottleStatus(base::HistogramTester* tester,
                          std::map<ClassifyUrlThrottleStatus, int> buckets) {
  int total = 0;
  for (const auto& [bucket, count] : buckets) {
    total += count;
    tester->ExpectBucketCount(kClassifyUrlThrottleStatusHistogramName, bucket,
                              count);
  }
  tester->ExpectTotalCount(kClassifyUrlThrottleStatusHistogramName, total);
}

void ExpectNoLatencyRecorded(base::HistogramTester* tester) {
  tester->ExpectTotalCount(kClassifiedEarlierThanContentResponseHistogramName,
                           /*expected_count=*/0);
  tester->ExpectTotalCount(kClassifiedLaterThanContentResponseHistogramName,
                           /*expected_count=*/0);
}

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
      const std::vector<GURL> redirects) {
    CHECK_GT(redirects.size(), 0U) << "At least one url is required";

    redirects_ = redirects;
    current_url_it_ = redirects_.begin();

    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            *current_url_it_, main_rfh());

    // Note: this creates the throttle regardless the supervision status of the
    // user.
    std::unique_ptr<content::NavigationThrottle> throttle =
        ClassifyUrlNavigationThrottle::MakeUnique(navigation_handle_.get());

    // Add mock handlers for resume & cancel deferred.
    throttle->set_resume_callback_for_testing(
        base::BindLambdaForTesting([&]() { resume_called_ = true; }));
    return throttle;
  }

  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    return CreateNavigationThrottle(std::vector<GURL>({url}));
  }

  // Advances the pointer of the current url internally and synchronizes the
  // navigation_handle_ accordingly: updating both the url and the redirect
  // chain that led to it.
  void AdvanceRedirect() {
    current_url_it_++;

    // CHECK_NE doesn't support std::vector::iterator comparison.
    CHECK_NE(redirects_.end() - current_url_it_, 0)
        << "Can't advance past last redirect";

    std::vector<GURL> redirect_chain;
    for (auto it = redirects_.begin(); it != current_url_it_; ++it) {
      redirect_chain.push_back(*it);
    }

    navigation_handle_->set_url(*current_url_it_);
    navigation_handle_->set_redirect_chain(redirect_chain);
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

  std::vector<GURL> redirects_;
  std::vector<GURL>::iterator current_url_it_;
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

  // This throttle continued on request, and proceeded on response.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 1},
                        {ClassifyUrlThrottleStatus::kProceed, 1}});
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

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle immediately deferred and presented an interstitial.
  ExpectThrottleStatus(
      histogram_tester(),
      {{ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial, 1}});
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

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle immediately deferred and presented an interstitial.
  ExpectThrottleStatus(
      histogram_tester(),
      {{ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial, 1}});
  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
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
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    GURL(kExampleURL), testing::_, false))
      .Times(1);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle immediately deferred and presented an interstitial.
  ExpectThrottleStatus(
      histogram_tester(),
      {{ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial, 1}});
  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
}

TEST_F(ClassifyUrlNavigationThrottleTest, ClassificationIsFasterThanHttp) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  MockSupervisedUserURLFilter::FilteringBehaviorCallback check;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&check](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            check = std::move(callback);
            return false;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    GURL(kExampleURL), testing::_, false))
      .Times(1);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());

  // Check is not completed yet
  EXPECT_TRUE(check);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // the check
  std::move(check).Run(FilteringBehavior::kAllow,
                       FilteringBehaviorReason::ASYNC_CHECKER,
                       /*is_uncertain=*/false);

  // Throttle is not blocked
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // As a result, the navigation hadn't had to be resumed
  EXPECT_FALSE(resume_called());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*grew_by=*/1);

  // This throttle continued on request, and proceeded on response because the
  // result was already there.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 1},
                        {ClassifyUrlThrottleStatus::kProceed, 1}});
}

TEST_F(ClassifyUrlNavigationThrottleTest, ClassificationIsSlowerThanHttp) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());
  MockSupervisedUserURLFilter::FilteringBehaviorCallback check;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&check](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            check = std::move(callback);
            return false;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    GURL(kExampleURL), testing::_, false))
      .Times(1);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GURL(kExampleURL));

  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());

  // At this point, check was not completed.
  EXPECT_TRUE(check);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // But will block at process response because the check is still
  // pending and no filtering was completed.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  // Now complete the outstanding check
  std::move(check).Run(FilteringBehavior::kAllow,
                       FilteringBehaviorReason::ASYNC_CHECKER,
                       /*is_uncertain=*/false);

  // As a result, the navigation is resumed (and three checks registered)
  EXPECT_TRUE(resume_called());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*grew_by=*/1);

  // This throttle continued on request, and deferred on response because the
  // result wasn't there. Then it resumed.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 1},
                        {ClassifyUrlThrottleStatus::kDefer, 1},
                        {ClassifyUrlThrottleStatus::kResume, 1}});
}

// Checks a scenario where the classification responses arrive in reverse order:
// Last check is completed first but is blocking, and first check is completed
// after it and is not blocking. Both checks complete after the response was
// ready for processing.
TEST_F(ClassifyUrlNavigationThrottleTest,
       ReverseOrderOfResponsesAfterContentIsReady) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());

  std::vector<MockSupervisedUserURLFilter::FilteringBehaviorCallback> checks;
  // Check for the first url that will complete last.
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, testing::_))
      .WillByDefault(
          [&checks](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            checks.push_back(std::move(callback));
            return false;
          });
  EXPECT_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                    testing::_, testing::_, false))
      .Times(2);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle({GURL(kExampleURL), GURL(kExample1URL)});

  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  // As expected, the process navigation is deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse());

  // Resolve pending checks in reverse order, so that block for 2nd request
  // comes first.
  std::move(checks[1]).Run(FilteringBehavior::kBlock,
                           FilteringBehaviorReason::ASYNC_CHECKER,
                           /*is_uncertain=*/false);
  std::move(checks[0]).Run(FilteringBehavior::kAllow,
                           FilteringBehaviorReason::ASYNC_CHECKER,
                           /*is_uncertain=*/false);

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle continued on request and redirect, and deferred on response
  // because the result wasn't there. It never recovered from defer state
  // (interstitial was presented).
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 2},
                        {ClassifyUrlThrottleStatus::kDefer, 1}});
  EXPECT_FALSE(resume_called());
}

struct TestCase {
  std::string name;
  std::vector<std::string> redirect_chain;
};

class ClassifyUrlNavigationThrottleParallelizationTest
    : public ClassifyUrlNavigationThrottleTest,
      public testing::WithParamInterface<TestCase> {
 protected:
  static const std::vector<GURL> GetRedirectChain() {
    CHECK_EQ(GetParam().redirect_chain.size(), 3U)
        << "Tests assume one request and two redirects";
    std::vector<GURL> urls;
    for (const auto& redirect : GetParam().redirect_chain) {
      urls.push_back(GURL(redirect));
    }
    return urls;
  }
};

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ClassificationIsFasterThanHttp) {
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
                                    testing::_, testing::_, false))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  AdvanceRedirect();
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

  // This throttle continued on request and redirects and proceeded because
  // verdict was ready.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 3},
                        {ClassifyUrlThrottleStatus::kProceed, 1}});
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       OutOfOrderClassification) {
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
                                    testing::_, testing::_, false))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());

  // No checks are completed yet
  EXPECT_THAT(checks, testing::SizeIs(3));
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // all checks but from the back.
  for (auto it = checks.rbegin(); it != checks.rend(); ++it) {
    std::move(*it).Run(FilteringBehavior::kAllow,
                       FilteringBehaviorReason::ASYNC_CHECKER,
                       /*is_uncertain=*/false);
    // Classification still not complete.
    histogram_tester()->ExpectTotalCount(
        kClassifiedEarlierThanContentResponseHistogramName,
        /*grew_by=*/0);
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

  // This throttle continued on request and redirects and then proceeded because
  // verdict was ready.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 3},
                        {ClassifyUrlThrottleStatus::kProceed, 1}});
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ClassificationIsSlowerThanHttp) {
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
                                    testing::_, testing::_, false))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  AdvanceRedirect();
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

  // This throttle continued on request and redirects and then deferred because
  // one check was outstanding. After it was completed, the throttle resumed.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 3},
                        {ClassifyUrlThrottleStatus::kDefer, 1},
                        {ClassifyUrlThrottleStatus::kResume, 1}});
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ShortCircuitsSynchronousBlock) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>(*profile()->GetPrefs());

  bool first_check = false;
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURLWithAsyncChecks(
                                testing::_, testing::_, false))
      .WillByDefault(
          [&first_check](
              const GURL& url,
              MockSupervisedUserURLFilter::FilteringBehaviorCallback callback,
              bool skip_manual_parent_filter) {
            if (!first_check) {
              std::move(callback).Run(FilteringBehavior::kAllow,
                                      FilteringBehaviorReason::ASYNC_CHECKER,
                                      /*is_uncertain=*/false);
              first_check = true;
              return true;
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
      CreateNavigationThrottle(GetRedirectChain());

  // It will DEFER at 2nd request (1st redirect).
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillRedirectRequest());

  // And one completed block from safe-sites (async checker)
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle continued on first request deferred on second one.
  ExpectThrottleStatus(
      histogram_tester(),
      {{ClassifyUrlThrottleStatus::kContinue, 1},
       {ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial, 1}});
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       HandlesLateAsynchronousBlock) {
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
                                    testing::_, testing::_, false))
      .Times(3);

  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::NavigationThrottle> throttle =
      CreateNavigationThrottle(GetRedirectChain());

  // It proceed all three request/redirects.
  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  AdvanceRedirect();

  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  AdvanceRedirect();

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
  ExpectNoLatencyRecorded(histogram_tester());
  // This throttle continued on request and redirects and deferred waiting for
  // last classification.
  ExpectThrottleStatus(histogram_tester(),
                       {{ClassifyUrlThrottleStatus::kContinue, 3},
                        {ClassifyUrlThrottleStatus::kDefer, 1}});
}

const TestCase kTestCases[] = {
    {.name = "TwoRedirects",
     .redirect_chain = {kExampleURL, kExample1URL, kExample2URL}},
    {.name = "TwoIdenticalRedirects",
     .redirect_chain = {kExampleURL, kExampleURL, kExampleURL}}};

INSTANTIATE_TEST_SUITE_P(,
                         ClassifyUrlNavigationThrottleParallelizationTest,
                         testing::ValuesIn(kTestCases),
                         [](const testing::TestParamInfo<TestCase>& info) {
                           return info.param.name;
                         });
}  // namespace supervised_user
