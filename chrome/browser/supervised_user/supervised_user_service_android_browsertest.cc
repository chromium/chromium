// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/features.h"
#include "components/url_matcher/url_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace supervised_user {
namespace {

using ::safe_search_api::ClientClassification;
using ::safe_search_api::URLCheckerClient;
using ::testing::_;

// Covers extra behaviors available only in Clank (Android) related to
// bootstrapping the supervised user service with Content Filters Observer (how
// the browser behaves after init, with no further manipulation of the content
// filters). The tests are parametrized so that they also try to "hot start" the
// browser, simulating that the browser thinks that it was previously
// supervised. To see tests that assert dynamic behaviors (when the filters are
// altered after the browser starts and urls are loaded), see
// supervised_user_navigation_observer_android_browsertest.cc
class SupervisedUserServiceBootstrapAndroidBrowserTestBase
    : public SupervisedUserBrowserTestBase {
 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    // Will resolve google.com to localhost, so the embedded test server can
    // serve some valid content for it.
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
    CHECK(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpCommandLine(command_line);
    // The production code only allows known ports (80 for http and 443 for
    // https), but the embedded test server runs on a random port and adds it to
    // the url spec.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  base::HistogramTester histogram_tester_;
};

struct BootstrapServiceTestCase {
  std::string test_name;
  // Determines the value of browser device filter on browser startup.
  bool initial_browser_content_filters_value;
  // Determines the value of search device filter on browser startup.
  bool initial_search_content_filters_value;

  // Returns true if incognito should be blocked based on the initial values of
  // the content filters settings.
  bool ShouldBlockIncognito() const {
    return initial_browser_content_filters_value ||
           initial_search_content_filters_value;
  }
};

// Tests the aspect where the Family Link supervision is not enabled, but the
// content filters are set.
class SupervisedUserServiceBootstrapAndroidBrowserTest
    : public WithFeatureOverrideAndParamInterface<BootstrapServiceTestCase>,
      public SupervisedUserServiceBootstrapAndroidBrowserTestBase {
 protected:
  SupervisedUserServiceBootstrapAndroidBrowserTest()
      : WithFeatureOverrideAndParamInterface<BootstrapServiceTestCase>(
            kSupervisedUserUseUrlFilteringService) {
    SetInitialSupervisedUserState(
        {.android_parental_controls = {
             .browser_filter =
                 GetTestCase().initial_browser_content_filters_value,
             .search_filter =
                 GetTestCase().initial_search_content_filters_value,
         }});
  }
};

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       IncognitoIsBlockedWhenAnyFilterIsEnabled) {
  ASSERT_NE(nullptr, GetSupervisedUserService());

  policy::IncognitoModeAvailability expected_incognito_mode_availability =
      GetTestCase().ShouldBlockIncognito()
          ? policy::IncognitoModeAvailability::kDisabled
          : policy::IncognitoModeAvailability::kEnabled;

  // TODO(http://crbug.com/433234589): this test could actually try to open
  // incognito (to no avail).
  EXPECT_EQ(expected_incognito_mode_availability,
            static_cast<policy::IncognitoModeAvailability>(
                GetProfile()->GetPrefs()->GetInteger(
                    policy::policy_prefs::kIncognitoModeAvailability)));
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       SafeSearchIsEnforcedWhenSearchFilterIsEnabled) {
  GURL request_url =
      embedded_test_server()->GetURL("google.com", "/search?q=cat");
  GURL expected_url = GetTestCase().initial_search_content_filters_value
                          ? GURL(request_url.spec() + "&safe=active&ssui=on")
                          : request_url;

  if (GetTestCase().initial_browser_content_filters_value) {
    // Google search is not on the exempt list of the URL Filter: search
    // requests must be explicitly allowed.
    EXPECT_CALL(GetMockUrlCheckerClient(),
                CheckURL(url_matcher::util::Normalize(expected_url), _))
        .WillOnce([](const GURL& url,
                     URLCheckerClient::ClientCheckCallback callback) {
          std::move(callback).Run(url, ClientClassification::kAllowed);
        });
  }

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), request_url, expected_url));
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       SafeSitesIsEnforcedWhenBrowserFilterIsEnabled) {
  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  if (GetTestCase().initial_browser_content_filters_value) {
    EXPECT_CALL(GetMockUrlCheckerClient(),
                CheckURL(url_matcher::util::Normalize(request_url), _))
        .WillOnce([](const GURL& url,
                     URLCheckerClient::ClientCheckCallback callback) {
          std::move(callback).Run(url, ClientClassification::kAllowed);
        });
  } else {
    EXPECT_CALL(GetMockUrlCheckerClient(),
                CheckURL(url_matcher::util::Normalize(request_url), _))
        .Times(0);
  }

  // We assert here (rather than expect) because url checker mock declares the
  // requested url as allowed (or never classified) so they should render at all
  // times. The core of this test is to count calls to the url checker client.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), request_url));
  ASSERT_EQ(web_contents()->GetTitle(), u"Supervised User test: simple page");
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       SafeSitesBlocksPagesWhenEnabled) {
  if (!GetTestCase().initial_browser_content_filters_value) {
    GTEST_SKIP() << "This test requires the browser filter to be enabled.";
  }

  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  EXPECT_CALL(GetMockUrlCheckerClient(),
              CheckURL(url_matcher::util::Normalize(request_url), _))
      .WillOnce(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(url, ClientClassification::kRestricted);
          });

  // We assert here (rather than expect) because url checker mock declares the
  // requested url as blocked. What we do care about is that the classification
  // was requested.
  ASSERT_FALSE(content::NavigateToURL(web_contents(), request_url));
  ASSERT_EQ(web_contents()->GetTitle(), u"Site blocked");
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       WebFilterTypeIsRecordedOnceWhenBrowserFilterIsEnabled) {
  if (GetTestCase().initial_browser_content_filters_value) {
    histogram_tester().ExpectBucketCount(
        "SupervisedUsers.WebFilterType.LocallySupervised",
        WebFilterType::kTryToBlockMatureSites, 1);
  } else if (GetTestCase().initial_search_content_filters_value) {
    // With url service enabled, when the search filter is enabled and the
    // browser filter is disabled, the web filter type indicates that it allows
    // all sites.
    WebFilterType expected_web_filter_type = IsFeatureEnabled()
                                                 ? WebFilterType::kAllowAllSites
                                                 : WebFilterType::kDisabled;
    histogram_tester().ExpectBucketCount(
        "SupervisedUsers.WebFilterType.LocallySupervised",
        expected_web_filter_type, 1);
  } else {
    histogram_tester().ExpectTotalCount(
        "SupervisedUsers.WebFilterType.LocallySupervised", 0);
  }

  // This histogram is not recorded for locally supervised users.
  histogram_tester().ExpectTotalCount("FamilyUser.WebFilterType", 0);
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       FamilyLinkOverridesDeviceSupervision) {
  bool is_initially_supervised_locally =
      GetTestCase().initial_browser_content_filters_value ||
      GetTestCase().initial_search_content_filters_value;

  // Device supervision is initially enabled/disabled based on the test case,
  // but Family Link supervision is always disabled.
  ASSERT_EQ(is_initially_supervised_locally,
            GetDeviceParentalControls().IsEnabled());
  ASSERT_FALSE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));

  // So far there is no trace of any supervision systems conflict.
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   "SupervisedUsers.FamilyLinkSupervisionConflict", 1));

  EnableParentalControls(*GetProfile()->GetPrefs());

  // Finally, local supervision is overridden (browser sees it as disabled),
  // Family Link supervision is always enabled, and if there was a conflict,
  // it's recorded (possibly multiple times, because changes to both
  // SupervisedUserSettingsService and AndroidParentalControls trigger pref
  // calculations)
  if (is_initially_supervised_locally) {
    EXPECT_GT(histogram_tester().GetBucketCount(
                  "SupervisedUsers.FamilyLinkSupervisionConflict", 1),
              0);
  } else {
    EXPECT_EQ(histogram_tester().GetBucketCount(
                  "SupervisedUsers.FamilyLinkSupervisionConflict", 1),
              0);
  }
  EXPECT_FALSE(
      AreAndroidParentalControlsEffectiveForTesting(*GetProfile()->GetPrefs()));
  EXPECT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));
}

const BootstrapServiceTestCase kBootstrapServiceTestCases[] = {
    {.test_name = "AllFiltersDisabled",
     .initial_browser_content_filters_value = false,
     .initial_search_content_filters_value = false},
    {.test_name = "AllFiltersEnabled",
     .initial_browser_content_filters_value = true,
     .initial_search_content_filters_value = true},
    {.test_name = "SearchFilterEnabled",
     .initial_browser_content_filters_value = false,
     .initial_search_content_filters_value = true},
    {.test_name = "BrowserFilterEnabled",
     .initial_browser_content_filters_value = true,
     .initial_search_content_filters_value = false}};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserServiceBootstrapAndroidBrowserTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kBootstrapServiceTestCases)),
    [](const testing::TestParamInfo<
        SupervisedUserServiceBootstrapAndroidBrowserTest::ParamType>& info) {
      bool feature_enabled = std::get<0>(info.param);
      BootstrapServiceTestCase test_case = std::get<1>(info.param);
      return base::StrCat({feature_enabled ? "With" : "Without",
                           kSupervisedUserUseUrlFilteringService.name, "_",
                           test_case.test_name});
    });

// Tests the aspect where the Family Link supervision is enabled, but the
// content filters are not set.
class SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest
    : public base::test::WithFeatureOverride,
      public SupervisedUserServiceBootstrapAndroidBrowserTestBase {
 protected:
  SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest()
      : base::test::WithFeatureOverride(kSupervisedUserUseUrlFilteringService) {
    SetInitialSupervisedUserState({.family_link_parental_controls = true});
  }
};

IN_PROC_BROWSER_TEST_P(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    IncognitoIsBlocked) {
  // TODO(http://crbug.com/433234589): this test could actually try to open
  // incognito (to no avail).
  EXPECT_EQ(static_cast<policy::IncognitoModeAvailability>(
                GetProfile()->GetPrefs()->GetInteger(
                    policy::policy_prefs::kIncognitoModeAvailability)),
            policy::IncognitoModeAvailability::kDisabled);
}

IN_PROC_BROWSER_TEST_P(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    SafeSitesBlocksPages) {
  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  EXPECT_CALL(GetMockUrlCheckerClient(),
              CheckURL(url_matcher::util::Normalize(request_url), _))
      .WillOnce(
          [](const GURL& url, URLCheckerClient::ClientCheckCallback callback) {
            std::move(callback).Run(url, ClientClassification::kRestricted);
          });

  // We assert here (rather than expect) because url checker mock declares the
  // requested url as blocked. What we do care about is that the classification
  // was requested.
  ASSERT_FALSE(content::NavigateToURL(web_contents(), request_url));
  ASSERT_EQ(web_contents()->GetTitle(), u"Site blocked");
}

IN_PROC_BROWSER_TEST_P(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    WebFilterTypeIsRecordedOnce) {
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 1);
  histogram_tester().ExpectBucketCount(
      "FamilyUser.WebFilterType", WebFilterType::kTryToBlockMatureSites, 1);
}

IN_PROC_BROWSER_TEST_P(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    FamilyLinkIsImmuneToDeviceSupervision) {
  // Device supervision is initially disabled and Family Link supervision is
  // initially enabled.
  ASSERT_FALSE(GetDeviceParentalControls().IsEnabled());
  ASSERT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));

  // Try turning the knob on the local supervision (browser filtering).
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_FALSE(
      AreAndroidParentalControlsEffectiveForTesting(*GetProfile()->GetPrefs()));
  EXPECT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.FamilyLinkSupervisionConflict", 1, 1);

  // Try turning the knob on the local supervision (search filtering).
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);
  EXPECT_FALSE(
      AreAndroidParentalControlsEffectiveForTesting(*GetProfile()->GetPrefs()));
  EXPECT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.FamilyLinkSupervisionConflict", 1, 2);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest);

// Tests the aspect where the Family Link supervision is disabled and the
// content filters are not set.
class SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest
    : public SupervisedUserServiceBootstrapAndroidBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest,
    IncognitoIsNotBlocked) {
  // TODO(http://crbug.com/433234589): this test could actually try to open
  // incognito (to no avail).
  EXPECT_EQ(static_cast<policy::IncognitoModeAvailability>(
                GetProfile()->GetPrefs()->GetInteger(
                    policy::policy_prefs::kIncognitoModeAvailability)),
            policy::IncognitoModeAvailability::kEnabled);
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest,
    SafeSitesIsNotUsed) {
  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");
  EXPECT_CALL(GetMockUrlCheckerClient(),
              CheckURL(url_matcher::util::Normalize(request_url), _))
      .Times(0);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), request_url));
  ASSERT_EQ(web_contents()->GetTitle(), u"Supervised User test: simple page");
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest,
    WebFilterTypeIsNotRecorded) {
  histogram_tester().ExpectTotalCount(
      "SupervisedUsers.WebFilterType.LocallySupervised", 0);
  histogram_tester().ExpectTotalCount("FamilyUser.WebFilterType", 0);
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest,
    SafeSearchIsNotEnforcedAtBrowserLevel) {
  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");

  EXPECT_CALL(GetMockUrlCheckerClient(),
              CheckURL(url_matcher::util::Normalize(url), _))
      .Times(0);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
}

}  // namespace
}  // namespace supervised_user
