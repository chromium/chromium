// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/features.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#endif

namespace supervised_user {

namespace {

static const char* kExampleURL = "https://example.com/";
static const char* kExample1URL = "https://example1.com/";
static const char* kExample2URL = "https://example2.com/";

void ExpectNoLatencyRecorded(base::HistogramTester* tester) {
  tester->ExpectTotalCount(kClassifiedEarlierThanContentResponseHistogramName,
                           /*expected_count=*/0);
  tester->ExpectTotalCount(kClassifiedLaterThanContentResponseHistogramName,
                           /*expected_count=*/0);
}

class ClassifyUrlNavigationThrottleTestBase
    : public ChromeRenderViewHostTestHarness {
 protected:
  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SupervisedUserServiceFactory::GetInstance(),
        base::BindRepeating(&ClassifyUrlNavigationThrottleTestBase::
                                BuildTestSupervisedUserService,
                            base::Unretained(this)));
    return builder.Build();
  }

  std::unique_ptr<content::MockNavigationThrottleRegistry>
  CreateNavigationThrottle(const std::vector<GURL> redirects) {
    CHECK_GT(redirects.size(), 0U) << "At least one url is required";

    redirects_ = redirects;
    current_url_it_ = redirects_.begin();

    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            *current_url_it_, main_rfh());

    // Note: this creates the throttle regardless the supervision status of the
    // user.
    auto registry = std::make_unique<content::MockNavigationThrottleRegistry>(
        navigation_handle_.get(),
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    ClassifyUrlNavigationThrottle::MaybeCreateAndAdd(*registry.get());

    if (!registry->throttles().empty()) {
      // Add mock handlers for resume & cancel deferred.
      registry->throttles().back()->set_resume_callback_for_testing(
          base::BindLambdaForTesting([&]() { resume_called_ = true; }));
    }
    return registry;
  }

  std::unique_ptr<content::MockNavigationThrottleRegistry>
  CreateNavigationThrottle(const GURL& url) {
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

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  bool resume_called() const { return resume_called_; }
  MockUrlCheckerClient& mock_url_checker_client() {
    return mock_url_checker_client_;
  }

 private:
  std::unique_ptr<KeyedService> BuildTestSupervisedUserService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess();
    FamilyLinkSettingsService& settings_service =
        CHECK_DEREF(FamilyLinkSettingsServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()));
    return std::make_unique<SupervisedUserService>(
        identity_manager, url_loader_factory, *profile->GetPrefs(),
        settings_service, SyncServiceFactory::GetForProfile(profile),
        std::make_unique<FamilyLinkUrlFilter>(
            settings_service, *profile->GetPrefs(),
            std::make_unique<FakeURLFilterDelegate>(),
            std::make_unique<UrlCheckerClientWrapper>(
                mock_url_checker_client_)),
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
        TestingBrowserProcess::GetGlobal()->device_parental_controls());
  }

  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
  base::HistogramTester histogram_tester_;
  bool resume_called_ = false;

  MockUrlCheckerClient mock_url_checker_client_;
  std::vector<GURL> redirects_;
  std::vector<GURL>::iterator current_url_it_;
};

// This test is used to test the behavior of the throttle when the user is not
// supervised - all navigations are allowed, but no metrics recorded.
class ClassifyUrlNavigationThrottleUnsupervisedUserTest
    : public base::test::WithFeatureOverride,
      public ClassifyUrlNavigationThrottleTestBase {
 protected:
  ClassifyUrlNavigationThrottleUnsupervisedUserTest()
      : base::test::WithFeatureOverride(kSupervisedUserUseUrlFilteringService) {
  }
  void SetUp() override { ClassifyUrlNavigationThrottleTestBase::SetUp(); }
};

TEST_P(ClassifyUrlNavigationThrottleUnsupervisedUserTest,
       WillNotRegisterThrottle) {
  EXPECT_TRUE(CreateNavigationThrottle(GURL(kExampleURL))->throttles().empty());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ClassifyUrlNavigationThrottleUnsupervisedUserTest);

class ClassifyUrlNavigationThrottleTest
    : public base::test::WithFeatureOverride,
      public ClassifyUrlNavigationThrottleTestBase {
 protected:
  ClassifyUrlNavigationThrottleTest()
      : base::test::WithFeatureOverride(kSupervisedUserUseUrlFilteringService) {
  }
  void SetUp() override {
    ClassifyUrlNavigationThrottleTestBase::SetUp();
    EnableParentalControls(*profile()->GetPrefs());
  }
};

TEST_P(ClassifyUrlNavigationThrottleTest, AllowedUrlsRecordedInAllowBucket) {
  GURL allowed_url(kExampleURL);
  supervised_user_test_util::SetManualFilterForHost(
      profile(), allowed_url.GetHost(), /*allowlist=*/true);

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(allowed_url);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillProcessResponse());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 1);

  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*expected_count(grew by)*/ 1);
}

TEST_P(ClassifyUrlNavigationThrottleTest,
       BlocklistedUrlsRecordedInBlockManualBucket) {
  GURL blocked_url(kExampleURL);
  supervised_user_test_util::SetManualFilterForHost(
      profile(), blocked_url.GetHost(), /*allowlist=*/false);
  ASSERT_TRUE(SupervisedUserUrlFilteringServiceFactory::GetForProfile(profile())
                  ->GetFilteringBehavior(blocked_url)
                  .IsBlocked());

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(blocked_url);
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockManual, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
}

TEST_P(ClassifyUrlNavigationThrottleTest,
       AllSitesBlockedRecordedInBlockNotInAllowlistBucket) {
  supervised_user_test_util::SetWebFilterType(profile(),
                                              WebFilterType::kCertainSites);

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ClassifyUrlNavigationThrottleTest);

enum class SupervisionMode {
  kSupervisedByFamilyLink,
#if BUILDFLAG(IS_ANDROID)
  kLocalSupervision,
#endif  // BUILDFLAG(IS_ANDROID)
};

struct AsyncCheckerTestCase {
  std::string name;
  SupervisionMode mode;
};

class ClassifyUrlNavigationThrottleAsyncCheckerTest
    : public WithFeatureOverrideAndParamInterface<AsyncCheckerTestCase>,
      public ClassifyUrlNavigationThrottleTestBase {
 protected:
  ClassifyUrlNavigationThrottleAsyncCheckerTest()
      : WithFeatureOverrideAndParamInterface(
            kSupervisedUserUseUrlFilteringService) {}

  void SetUp() override {
    ClassifyUrlNavigationThrottleTestBase::SetUp();
    switch (GetTestCase().mode) {
      case SupervisionMode::kSupervisedByFamilyLink:
        EnableParentalControls(*profile()->GetPrefs());
        break;
#if BUILDFLAG(IS_ANDROID)
      case SupervisionMode::kLocalSupervision:
        if (IsFeatureEnabled()) {
          GTEST_SKIP() << "Not implemented. Local parental controls are no "
                          "longer handled by the pref store (exclusively with "
                          "Family Link), and new url filtering service "
                          "implementation does not support this mode yet.";
        }

        TestingBrowserProcess::GetGlobal()
            ->android_parental_controls()
            .SetBrowserContentFiltersEnabledForTesting(true);
        break;
#endif  // BUILDFLAG(IS_ANDROID)
    }
  }
};

TEST_P(ClassifyUrlNavigationThrottleAsyncCheckerTest,
       BlockedMatureSitesRecordedInBlockSafeSitesBucket) {
  EXPECT_CALL(mock_url_checker_client(),
              CheckURL(GURL(kExampleURL), testing::_))
      .WillOnce(
          [](const GURL& url,
             safe_search_api::URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(
                url, safe_search_api::ClientClassification::kRestricted);
          });
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillStartRequest());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
}

TEST_P(ClassifyUrlNavigationThrottleAsyncCheckerTest,
       ClassificationIsFasterThanHttp) {
  EXPECT_CALL(mock_url_checker_client(),
              CheckURL(GURL(kExampleURL), testing::_))
      .Times(1);

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());

  // Check is not completed yet
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // the check
  mock_url_checker_client().RunFirstCallack(
      safe_search_api::ClientClassification::kAllowed);

  // Throttle is not blocked
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillProcessResponse());

  // As a result, the navigation hadn't had to be resumed
  EXPECT_FALSE(resume_called());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 1);

  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*expected_count=*/1);
}

TEST_P(ClassifyUrlNavigationThrottleAsyncCheckerTest,
       ClassificationIsSlowerThanHttp) {
  EXPECT_CALL(mock_url_checker_client(),
              CheckURL(GURL(kExampleURL), testing::_))
      .Times(1);

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));

  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());

  // At this point, check was not completed.
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // But will block at process response because the check is still
  // pending and no filtering was completed.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillProcessResponse());

  // Now complete the outstanding check
  mock_url_checker_client().RunFirstCallack(
      safe_search_api::ClientClassification::kAllowed);

  // As a result, the navigation is resumed (and three checks registered)
  EXPECT_TRUE(resume_called());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 1);

  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*expected_count=*/1);
}

// Checks a scenario where the classification responses arrive in reverse order:
// Last check is completed first but is blocking, and first check is completed
// after it and is not blocking. Both checks complete after the response was
// ready for processing.
TEST_P(ClassifyUrlNavigationThrottleAsyncCheckerTest,
       ReverseOrderOfResponsesAfterContentIsReady) {
  EXPECT_CALL(mock_url_checker_client(),
              CheckURL(GURL(kExampleURL), testing::_))
      .Times(1);
  EXPECT_CALL(mock_url_checker_client(),
              CheckURL(GURL(kExample1URL), testing::_))
      .Times(1);

  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle({GURL(kExampleURL), GURL(kExample1URL)});

  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());
  // As expected, the process navigation is deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillProcessResponse());

  // Resolve pending checks in reverse order, so that block for 2nd request
  // comes first.
  mock_url_checker_client().RunLastCallack(
      safe_search_api::ClientClassification::kRestricted);
  mock_url_checker_client().RunLastCallack(
      safe_search_api::ClientClassification::kAllowed);

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
  EXPECT_FALSE(resume_called());
}

const AsyncCheckerTestCase kAsyncCheckerTestCases[] = {
    {.name = "SupervisedByFamilyLink",
     .mode = SupervisionMode::kSupervisedByFamilyLink}
#if BUILDFLAG(IS_ANDROID)
    ,
    {.name = "LocalSupervision", .mode = SupervisionMode::kLocalSupervision}
#endif  // BUILDFLAG(IS_ANDROID)
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ClassifyUrlNavigationThrottleAsyncCheckerTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kAsyncCheckerTestCases)),
    [](const auto& info) {
      bool is_feature_enabled = std::get<0>(info.param);
      return std::get<1>(info.param).name + "_With" +
             kSupervisedUserUseUrlFilteringService.name +
             (is_feature_enabled ? "Enabled" : "Disabled");
    });

struct ParallelizationTestCase {
  std::string name;
  std::vector<std::string> redirect_chain;
};

class ClassifyUrlNavigationThrottleParallelizationTest
    : public WithFeatureOverrideAndParamInterface<ParallelizationTestCase>,
      public ClassifyUrlNavigationThrottleTestBase {
 protected:
  ClassifyUrlNavigationThrottleParallelizationTest()
      : WithFeatureOverrideAndParamInterface(
            kSupervisedUserUseUrlFilteringService) {}

  void SetUp() override {
    ClassifyUrlNavigationThrottleTestBase::SetUp();
    EnableParentalControls(*profile()->GetPrefs());
  }

  static const std::vector<GURL> GetRedirectChain() {
    CHECK_EQ(GetTestCase().redirect_chain.size(), 3U)
        << "Tests assume one request and two redirects";
    std::vector<GURL> urls;
    for (const std::string& redirect : GetTestCase().redirect_chain) {
      urls.emplace_back(redirect);
    }
    return urls;
  }
};

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ClassificationIsFasterThanHttp) {
  // safe_search_api::URLChecker has cache that deduplicates urls, so ultimately
  // the checker client is only for unique URLs.
  std::set<std::string> unique_urls(GetTestCase().redirect_chain.begin(),
                                    GetTestCase().redirect_chain.end());
  EXPECT_CALL(mock_url_checker_client(), CheckURL(testing::_, testing::_))
      .Times(unique_urls.size());

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());

  // No checks are completed yet
  std::size_t pending_checks_count =
      mock_url_checker_client().GetPendingChecksCount();
  EXPECT_EQ(unique_urls.size(), pending_checks_count);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // all pending checks for all redirects.
  for (std::size_t i = 0; i < pending_checks_count; ++i) {
    mock_url_checker_client().RunFirstCallack(
        safe_search_api::ClientClassification::kAllowed);
  }

  // Throttle is not blocked
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillProcessResponse());

  // As a result, the navigation hadn't had to be resumed
  EXPECT_FALSE(resume_called());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 3);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 3);

  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*expected_count=*/1);
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       OutOfOrderClassification) {
  // safe_search_api::URLChecker has cache that deduplicates urls, so ultimately
  // the checker client is only for unique URLs.
  std::set<std::string> unique_urls(GetTestCase().redirect_chain.begin(),
                                    GetTestCase().redirect_chain.end());
  EXPECT_CALL(mock_url_checker_client(), CheckURL(testing::_, testing::_))
      .Times(unique_urls.size());

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());

  // No checks are completed yet
  std::size_t pending_checks_count =
      mock_url_checker_client().GetPendingChecksCount();
  EXPECT_EQ(unique_urls.size(), pending_checks_count);
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Before the throttle will be notified that the content is ready, complete
  // all checks but from the back.
  for (std::size_t i = 0; i < pending_checks_count; ++i) {
    mock_url_checker_client().RunLastCallack(
        safe_search_api::ClientClassification::kAllowed);
    // Classification still not complete.
    histogram_tester()->ExpectTotalCount(
        kClassifiedEarlierThanContentResponseHistogramName,
        /*expected_count=*/0);
  }

  // Throttle is not blocked
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillProcessResponse());

  // As a result, the navigation hadn't had to be resumed
  EXPECT_FALSE(resume_called());

  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 3);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 3);

  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedEarlierThanContentResponseHistogramName,
      /*expected_count=*/1);
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ClassificationIsSlowerThanHttp) {
  // safe_search_api::URLChecker has cache that deduplicates urls, so ultimately
  // the checker client is only for unique URLs.
  std::set<std::string> unique_urls(GetTestCase().redirect_chain.begin(),
                                    GetTestCase().redirect_chain.end());
  EXPECT_CALL(mock_url_checker_client(), CheckURL(testing::_, testing::_))
      .Times(unique_urls.size());

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GetRedirectChain());

  // It will allow request and two redirects to pass...
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());
  AdvanceRedirect();
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());

  // At this point, no check was completed.
  EXPECT_EQ(unique_urls.size(),
            mock_url_checker_client().GetPendingChecksCount());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 0);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 0);

  // Complete all but first check
  {
    std::size_t pending_checks_count =
        mock_url_checker_client().GetPendingChecksCount();
    for (std::size_t i = 1; i < pending_checks_count; ++i) {
      mock_url_checker_client().RunLastCallack(
          safe_search_api::ClientClassification::kAllowed);
    }
  }

  // Now only one check is pending and the rest are completed.
  EXPECT_EQ(std::size_t(1), mock_url_checker_client().GetPendingChecksCount());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, unique_urls.size() - 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, unique_urls.size() - 1);

  // But will block at process response because one check is still
  // pending and no filtering was completed.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillProcessResponse());

  // Now complete the outstanding check
  mock_url_checker_client().RunLastCallack(
      safe_search_api::ClientClassification::kAllowed);

  // As a result, the navigation is resumed (and three checks registered, even
  // duplicated, because supervised user stack treats each navigation
  // independently).
  EXPECT_TRUE(resume_called());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 3);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 3);

  // Since the throttle had to wait for checks to complete, it recorded a
  // corresponding metric.
  histogram_tester()->ExpectTotalCount(
      kClassifiedLaterThanContentResponseHistogramName,
      /*expected_count=*/1);
}

// Verifies if the throttle will issue a blocking verdict as soon as it realizes
// that outstanding checks won't make a difference. In this case, the first
// blocking classification renders "blocking" verdict, and the rest of
// classifications are not important anymore.
TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       ShortCircuitsSynchronousBlock) {
  EXPECT_CALL(mock_url_checker_client(), CheckURL(testing::_, testing::_))
      .Times(1);
  mock_url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kRestricted);

  // This navigation is a 3-piece redirect chain:
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GetRedirectChain());

  // It will DEFER at 1st request (to show interstitial).
  ASSERT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillStartRequest());

  // And one completed block from safe-sites (async checker)
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
}

TEST_P(ClassifyUrlNavigationThrottleParallelizationTest,
       HandlesLateAsynchronousBlock) {
  // safe_search_api::URLChecker has cache that deduplicates urls, so ultimately
  // the checker client is only for unique URLs.
  std::set<std::string> unique_urls(GetTestCase().redirect_chain.begin(),
                                    GetTestCase().redirect_chain.end());
  if (unique_urls.size() == 1) {
    GTEST_SKIP()
        << "This test requires at least two unique URLs to test the "
           "caching behavior of the throttle against the URLCheckerClient.";
  }

  EXPECT_CALL(mock_url_checker_client(), CheckURL(testing::_, testing::_))
      .Times(unique_urls.size());

  mock_url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kAllowed);

  // This navigation is a 3-piece redirect chain on the same URL:
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GetRedirectChain());

  // It proceed all three request/redirects.
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillStartRequest());
  AdvanceRedirect();

  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());
  AdvanceRedirect();

  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry->throttles().back()->WillRedirectRequest());

  // There will be two pending checks (first was synchronous)
  EXPECT_EQ(std::size_t(2), mock_url_checker_client().GetPendingChecksCount());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kAllow, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kAllow, 1);

  // Http server completes first
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            registry->throttles().back()->WillProcessResponse());

  // Complete first pending check
  mock_url_checker_client().RunFirstCallack(
      safe_search_api::ClientClassification::kRestricted);

  // Now two out of three checks are complete
  EXPECT_EQ(std::size_t(1), mock_url_checker_client().GetPendingChecksCount());
  histogram_tester()->ExpectBucketCount(
      kSupervisedUserTopLevelURLFilteringResult2HistogramName,
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
  histogram_tester()->ExpectBucketCount(
      "SupervisedUsers.All.TopLevelFilteringResult.NavigationThrottle",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);

  // As a result, the navigation is not resumed
  EXPECT_FALSE(resume_called());
  // Since this is not a success path, no latency metric is recorded.
  ExpectNoLatencyRecorded(histogram_tester());
}

const ParallelizationTestCase kTestCases[] = {
    {.name = "TwoRedirects",
     .redirect_chain = {kExampleURL, kExample1URL, kExample2URL}},
    {.name = "TwoIdenticalRedirects",
     .redirect_chain = {kExampleURL, kExampleURL, kExampleURL}}};

INSTANTIATE_TEST_SUITE_P(,
                         ClassifyUrlNavigationThrottleParallelizationTest,
                         testing::Combine(testing::Bool(),
                                          testing::ValuesIn(kTestCases)),
                         [](const auto& info) {
                           bool is_feature_enabled = std::get<0>(info.param);
                           return std::get<1>(info.param).name + "_With" +
                                  kSupervisedUserUseUrlFilteringService.name +
                                  (is_feature_enabled ? "Enabled" : "Disabled");
                         });

}  // namespace
}  // namespace supervised_user
