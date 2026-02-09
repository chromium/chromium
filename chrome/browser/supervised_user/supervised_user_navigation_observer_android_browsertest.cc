// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

using ::safe_search_api::ClientClassification;
using ::safe_search_api::URLCheckerClient;
using ::testing::_;

struct TestCase {
  std::string test_label;
  bool start_with_family_link_enabled;
  bool merge_device_parental_controls_and_family_link_prefs;
};

// Test case where the system has Family Link parental controls enabled but
// won't merge with device parental controls is impossible: in this
// configuration both systems are mutually exclusive and Device Parental
// Controls will be ignored.
TestCase kTestCases[] = {
    {"StartWithFamilyLinkEnabled_MergeWithDeviceParentalControls", true, true},
    {"StartWithFamilyLinkDisabled_MergeWithDeviceParentalControls", false,
     true},
    {"StartWithFamilyLinkDisabled_DoNotMergeWithDeviceParentalControls", false,
     false},
};

// Covers extra behaviors available only in Clank (Android). See supervised
// user navigation and throttle tests for general behavior.
// WithFeatureOverrideAndParamInterface is used for
// kSupervisedUserUseUrlFilteringService, which should be neutral in any setting
// to this test.
class SupervisedUserNavigationObserverAndroidBrowserTest
    : public WithFeatureOverrideAndParamInterface<TestCase>,
      public SupervisedUserBrowserTestBase {
 protected:
  SupervisedUserNavigationObserverAndroidBrowserTest()
      : WithFeatureOverrideAndParamInterface<TestCase>(
            kSupervisedUserUseUrlFilteringService) {
#if BUILDFLAG(IS_ANDROID)
    // On Android, we disable the feature that automatically scales web content
    // because it is not meaningful and would change expected values.
    scoped_feature_list_.InitWithFeatureStates(
        {{kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs,
          GetTestCase().merge_device_parental_controls_and_family_link_prefs},
         { features::kAndroidDesktopZoomScaling,
           false }});
#else
    scoped_feature_list_.InitWithFeatureState(
        kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs,
        GetTestCase().merge_device_parental_controls_and_family_link_prefs);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    // Will resolve google.com to localhost, so the embedded test server can
    // serve a valid content for it.
    host_resolver()->AddRule("google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL().GetPath() != "/search") {
            return nullptr;
          }
          // HTTP 200 OK with empty response body.
          return std::make_unique<net::test_server::BasicHttpResponse>();
        }));
    ASSERT_TRUE(embedded_test_server()->Start());

    if (GetTestCase().start_with_family_link_enabled) {
      // For Family Link users, we need to do a few tweaks:
      // 1. Set the URL classification to "allow all" mode so that the system
      // won't try to classify google.com (blocking the navigation).
      // 2. Set the supervised user auth throttle to pass through test only
      // mode, so it won't require user reauthentication (blocking the
      // navigation).
      Profile* profile = chrome_test_utils::GetProfile(this);
      supervised_user_test_util::SetManualFilterForHost(profile, "google.com",
                                                        /*allowlist=*/true);
      SupervisedUserGoogleAuthNavigationThrottle::SetPassThrottleForTesting();
      EnableParentalControls(*profile->GetPrefs());
    } else {
      DisableParentalControls(
          *Profile::FromBrowserContext(web_contents()->GetBrowserContext())
               ->GetPrefs());
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpCommandLine(command_line);
    // The production code only allows known ports (80 for http and 443 for
    // https), but the embedded test server runs on a random port and adds it
    // to the url spec.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With disabled search content filters, the navigation is unchanged and safe
// search query params are not appended.
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationObserverAndroidBrowserTest,
                       DontPropagateSearchContentFilterSettingWhenDisabled) {
  ASSERT_FALSE(GetDeviceParentalControls().IsSearchContentFiltersEnabled());

  // The loaded URL is exactly as requested.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("google.com", "/search?q=cat")));
}

// Verifies that the search content filter setting is propagated through the
// supervised user service to navigation throttles that alter the URL. This
// particular test doesn't require navigation observer, but is hosted here for
// feature consistency.
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationObserverAndroidBrowserTest,
                       LoadSafeSearchResultsWithSearchContentFilterPreset) {
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);
  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");

  // The final url will be different: with safe search query params.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), url, GURL(url.spec() + "&safe=active&ssui=on")));
}

// Anti-regression test.
// Inactive supervised user settings must not affect the values set in the
// supervised user pref store by device parental controls.
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationObserverAndroidBrowserTest,
                       InactiveSupervisedUserSettingsCantVetoSafeSearch) {
  if (GetTestCase().merge_device_parental_controls_and_family_link_prefs &&
      GetTestCase().start_with_family_link_enabled) {
    GTEST_SKIP() << "This test specifically tests what happens when the Family "
                    "Link parental controls are enabled after Device Parental "
                    "Controls were set; and there's no merging of settings.";
  }

  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  FamilyLinkSettingsService* family_link_settings_service =
      FamilyLinkSettingsServiceFactory::GetForKey(profile->GetProfileKey());

  // Settings service should be already inactive (because family link parental
  // controls were never activated).
  ASSERT_FALSE(IsSubjectToParentalControls(*profile->GetPrefs()));
  ASSERT_FALSE(family_link_settings_service->IsActive());
  // This means that this call should be a no-op if service was previously
  // inactive. If it was not a no-op, it would clear the supervised user prefs
  // store and consequently the safe search setting from device parental
  // controls.
  family_link_settings_service->SetActive(false);

  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");

  // Despite inactivating the settings service, the final url still contains
  // safe search extra query params - it's because the inactivation was a
  // no-op.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), url, GURL(url.spec() + "&safe=active&ssui=on")));
}

// Similar to the above test, but the URL already contains safe search query
// params (for example, from a previous navigation or added manually by user
// in the Omnibox). They are removed regardless of their value, and safe
// search params are appended.
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationObserverAndroidBrowserTest,
                       PreexistingSafeSearchParamsAreRemovedBeforeAppending) {
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);
  GURL url = embedded_test_server()->GetURL("google.com",
                                            "/search?safe=off&ssui=on&q=cat");

  // The final url will be different: with extra query params appended and
  // previous ones removed.
  GURL expected_url = embedded_test_server()->GetURL(
      "google.com", "/search?q=cat&safe=active&ssui=on");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url, expected_url));
}

// Verifies that the search content filter is propagated through the
// supervised user service to to the navigation observer, and that the
// navigation observer triggers the page reload.
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationObserverAndroidBrowserTest,
                       ReloadSearchResultAfterSearchContentFilterIsEnabled) {
  // Verify that the observer is attached.
  ASSERT_NE(nullptr,
            SupervisedUserNavigationObserver::FromWebContents(web_contents()));

  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  content::TestNavigationObserver navigation_observer(web_contents());
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);
  navigation_observer.Wait();

  // Key part: the search results are reloaded with extra query params.
  EXPECT_EQ(url.spec() + "&safe=active&ssui=on",
            web_contents()->GetLastCommittedURL());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationObserverAndroidBrowserTest,
    testing::Combine(testing::Bool(), testing::ValuesIn(kTestCases)),
    [](const testing::TestParamInfo<
        SupervisedUserNavigationObserverAndroidBrowserTest::ParamType>& info) {
      bool feature_enabled = std::get<0>(info.param);
      TestCase test_case = std::get<1>(info.param);
      return base::StrCat({feature_enabled ? "With" : "Without",
                           kSupervisedUserUseUrlFilteringService.name, "_",
                           test_case.test_label});
    });

// Tests if no-approval interstitial is shown when the browser content filter
// is enabled.
class SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest
    : public SupervisedUserNavigationObserverAndroidBrowserTest {
 protected:
  void EnableBrowserFilteringAndWaitForInterstitial() {
    content::TestNavigationObserver navigation_observer(web_contents());
    // Turn the filtering on. That will trigger a url check which is resolved
    // to restricted.
    GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);
    navigation_observer.Wait();
  }

  void ClickButtonById(std::string_view link_id) {
    content::TestNavigationObserver navigation_observer(web_contents());
    content::SimulateMouseClickOrTapElementWithId(web_contents(), link_id);
    navigation_observer.Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_ANDROID)
    // On Android, we disable the feature that automatically scales web content
    // because it is not meaningful and would change expected values.
    scoped_feature_list_.InitWithFeatureStates(
        {{ features::kAndroidDesktopZoomScaling,
           false }});
#endif  // BUILDFLAG(IS_ANDROID)
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Shows the interstitial page when the search content filter is enabled.
IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest,
    ShowInterstitialPage) {
  // Verify that the observer is attached.
  ASSERT_NE(nullptr,
            SupervisedUserNavigationObserver::FromWebContents(web_contents()));
  GURL url = embedded_test_server()->GetURL("/supervised_user/simple.html");

  // In this test, all classifications are restricted after enabling the
  // browser content filter.
  EXPECT_CALL(GetMockUrlCheckerClient(), CheckURL(url, _))
      .WillOnce(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(url, ClientClassification::kRestricted);
          });

  // Navigate to a simple page and verify the title. The page is not filtered.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_EQ(u"Supervised User test: simple page", web_contents()->GetTitle());

  EnableBrowserFilteringAndWaitForInterstitial();

  EXPECT_EQ(u"Site blocked", web_contents()->GetTitle());
  // Learn more button is specific to this interstitial.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('learn-more-button');"));
}

// Clicks the learn more button on the interstitial page and verifies that the
// help center page about to be loaded.
IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest,
    GoToHelpCenterPage) {
  // Verify that the observer is attached.
  ASSERT_NE(nullptr,
            SupervisedUserNavigationObserver::FromWebContents(web_contents()));

  // In this test, all classifications are restricted after enabling the
  // browser content filter.
  ON_CALL(GetMockUrlCheckerClient(), CheckURL)
      .WillByDefault(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            if (url.host() == GURL(kDeviceFiltersHelpCenterUrl).host()) {
              // Required, because device parental controls url filter does not
              // statically except Google-owned domains (eg. the help center
              // page) - it does not have to, because device parental controls
              // don't offer option to accidentally block list these urls.
              std::move(callback).Run(url, ClientClassification::kAllowed);
              return;
            }
            std::move(callback).Run(url, ClientClassification::kRestricted);
          });

  // Navigate to a simple page and verify the title. The page is not filtered.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("/supervised_user/simple.html")));
  ASSERT_EQ(u"Supervised User test: simple page", web_contents()->GetTitle());

  // After filters are enabled, the interstitial page is shown.
  EnableBrowserFilteringAndWaitForInterstitial();
  EXPECT_EQ(u"Site blocked", web_contents()->GetTitle());

  // Navigation to google.com pages is expected to be always allowed.
  GURL help_center_url = GURL(kDeviceFiltersHelpCenterUrl);
  ASSERT_TRUE(GetSupervisedUserUrlFilteringService()
                  ->GetFilteringBehavior(help_center_url)
                  .IsAllowed());

  histogram_tester().ExpectTotalCount("ManagedMode.BlockingInterstitialCommand",
                                      0);
  ClickButtonById("learn-more-button");
  histogram_tester().ExpectBucketCount(
      "ManagedMode.BlockingInterstitialCommand",
      SupervisedUserInterstitial::Commands::LEARN_MORE, 1);

  // This expectation verifies that the help center page was attempted to be
  // loaded (test don't have internet)
  EXPECT_EQ(base::UTF8ToUTF16(help_center_url.GetHost()),
            web_contents()->GetTitle());
}

// Clicks the back button on the interstitial page and verifies that the
// previous page is shown again.
IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest,
    GoBack) {
  // Verify that the observer is attached.
  ASSERT_NE(SupervisedUserNavigationObserver::FromWebContents(web_contents()),
            nullptr);

  GURL allowed_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");
  GURL blocked_url =
      embedded_test_server()->GetURL("/supervised_user/explicit.html");

  // In this test to facilitate the back button click, one url is allowed but
  // others are not. All navigations are subject to classification in this
  // test.
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);

  // Two classification calls are expected:
  // 1. when the page is first loaded
  // 2. when the explicit page is attempted to be loaded
  // Back button click is not triggering any classification requests.
  EXPECT_CALL(GetMockUrlCheckerClient(), CheckURL(allowed_url, _))
      .WillOnce(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(url, ClientClassification::kAllowed);
          });
  EXPECT_CALL(GetMockUrlCheckerClient(), CheckURL(blocked_url, _))
      .WillOnce(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(url, ClientClassification::kRestricted);
          });

  // Navigate to a simple page and verify the title. The page is not filtered.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), allowed_url));
  ASSERT_EQ(u"Supervised User test: simple page", web_contents()->GetTitle());

  // Navigate to blocked url. The navigation is not successful even though the
  // url is committed, because the interstitial blocks it.
  EXPECT_FALSE(content::NavigateToURL(web_contents(), blocked_url));
  EXPECT_EQ(u"Site blocked", web_contents()->GetTitle());
  EXPECT_EQ(blocked_url, web_contents()->GetLastCommittedURL());

  // After clicking the back button, the previous page is available back
  // again.
  histogram_tester().ExpectTotalCount("ManagedMode.BlockingInterstitialCommand",
                                      0);
  ClickButtonById("back-button");
  histogram_tester().ExpectBucketCount(
      "ManagedMode.BlockingInterstitialCommand",
      SupervisedUserInterstitial::Commands::BACK, 1);

  EXPECT_EQ(u"Supervised User test: simple page", web_contents()->GetTitle());
  EXPECT_EQ(allowed_url, web_contents()->GetLastCommittedURL());
}

// TODO(crbug.com/477959773): Add test cases which will show either Family Link
// or Device Parental Controls interstitial (depending on which system blocked
// the navigation).
TestCase kTestCasesNoApprovalsInterstitial[] = {
    {"StartWithFamilyLinkDisabled_MergeWithDeviceParentalControls", false,
     true},
    {"StartWithFamilyLinkDisabled_DoNotMergeWithDeviceParentalControls", false,
     false},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kTestCasesNoApprovalsInterstitial)),
    [](const testing::TestParamInfo<
        SupervisedUserNavigationObserverNoApprovalsInterstitialAndroidBrowserTest::
            ParamType>& info) {
      bool feature_enabled = std::get<0>(info.param);
      TestCase test_case = std::get<1>(info.param);
      return base::StrCat({feature_enabled ? "With" : "Without",
                           kSupervisedUserUseUrlFilteringService.name, "_",
                           test_case.test_label});
    });

}  // namespace
}  // namespace supervised_user
