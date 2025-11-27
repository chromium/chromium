// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

namespace supervised_user {
namespace {

using ::safe_search_api::ClientClassification;
using ::safe_search_api::URLCheckerClient;
using ::testing::_;

class MockUrlCheckerClient : public URLCheckerClient {
 public:
  MOCK_METHOD(void, CheckURL, (const GURL& url, ClientCheckCallback callback));
};

// Covers extra behaviors available only in Clank (Android) related to
// bootstrapping the supervised user service with Content Filters Observer (how
// the browser behaves after init, with no further manipulation of the content
// filters). The tests are parametrized so that they also try to "hot start" the
// browser, simulating that the browser thinks that it was previously
// supervised. To see tests that assert dynamic behaviors (when the filters are
// altered after the browser starts and urls are loaded), see
// supervised_user_navigation_observer_android_browsertest.cc
class SupervisedUserServiceBootstrapAndroidBrowserTestBase
    : public AndroidBrowserTest {
 protected:
  // Creates a fake content filters observer bridge for testing, and binds it to
  // this test fixture.
  virtual std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls) = 0;

  // Called just before supervised user service is created. Much like
  // SetUpLocalStatePrefService, but called after prefs are registered.
  virtual void SetUpPrefs(PrefService* local_state) {}

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  MockUrlCheckerClient* url_checker_client() { return url_checker_client_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TestSupervisedUserService* GetTestSupervisedUserService() {
    return static_cast<TestSupervisedUserService*>(
        SupervisedUserServiceFactory::GetForProfile(GetProfile()));
  }

 private:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AndroidBrowserTest::SetUpBrowserContextKeyedServices(context);
    SupervisedUserServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &SupervisedUserServiceBootstrapAndroidBrowserTestBase::
                         BuildSupervisedUserService,
                     base::Unretained(this)));
  }

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

  // Builds a SupervisedUserService with a fake content filters observer bridge
  // that bootstraps with initial values from the test case.
  std::unique_ptr<KeyedService> BuildSupervisedUserService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    SetUpPrefs(profile->GetPrefs());

    std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);

    std::unique_ptr<MockUrlCheckerClient> url_checker_client =
        std::make_unique<MockUrlCheckerClient>();
    url_checker_client_ = url_checker_client.get();

    return std::make_unique<TestSupervisedUserService>(
        IdentityManagerFactory::GetForProfile(profile),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        *profile->GetPrefs(),
        *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()),
        SupervisedUserContentFiltersServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()),
        SyncServiceFactory::GetInstance()->GetForProfile(profile),
        std::make_unique<SupervisedUserURLFilter>(
            *profile->GetPrefs(), std::make_unique<FakeURLFilterDelegate>(),
            std::move(url_checker_client)),
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
        base::BindRepeating(
            &SupervisedUserServiceBootstrapAndroidBrowserTestBase::CreateBridge,
            base::Unretained(this)));
  }

  base::HistogramTester histogram_tester_;
  raw_ptr<MockUrlCheckerClient> url_checker_client_;
  base::test::ScopedFeatureList scoped_feature_list_{
      kPropagateDeviceContentFiltersToSupervisedUser};
};

struct BootstrapServiceTestCase {
  std::string test_name;
  // Determines the value of browser device filter on browser startup.
  bool initial_browser_content_filters_value;
  // Determines the value of search device filter on browser startup.
  bool initial_search_content_filters_value;

  // Returns the initial value for the given content filters setting.
  bool ResolveInitialValueForFilter(std::string_view setting_name) const {
    if (setting_name == kBrowserContentFiltersSettingName) {
      return initial_browser_content_filters_value;
    }
    if (setting_name == kSearchContentFiltersSettingName) {
      return initial_search_content_filters_value;
    }
    NOTREACHED() << "Unsupported setting name: " << setting_name;
  }

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
    : public SupervisedUserServiceBootstrapAndroidBrowserTestBase,
      public ::testing::WithParamInterface<BootstrapServiceTestCase> {
 protected:
  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls)
      override {
    return std::make_unique<FakeContentFiltersObserverBridge>(
        setting_name, on_enabled, on_disabled, is_subject_to_parental_controls,
        GetParam().ResolveInitialValueForFilter(setting_name));
  }
};

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       IncognitoIsBlockedWhenAnyFilterIsEnabled) {
  policy::IncognitoModeAvailability expected_incognito_mode_availability =
      GetParam().ShouldBlockIncognito()
          ? policy::IncognitoModeAvailability::kDisabled
          : policy::IncognitoModeAvailability::kEnabled;

  // TODO(http://crbug.com/433234589): this test could actually try to open
  // incognito (to no avail).
  EXPECT_EQ(static_cast<policy::IncognitoModeAvailability>(
                GetProfile()->GetPrefs()->GetInteger(
                    policy::policy_prefs::kIncognitoModeAvailability)),
            expected_incognito_mode_availability);
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       SafeSearchIsEnforcedWhenSearchFilterIsEnabled) {
  GURL request_url =
      embedded_test_server()->GetURL("google.com", "/search?q=cat");
  GURL expected_url = GetParam().initial_search_content_filters_value
                          ? GURL(request_url.spec() + "&safe=active&ssui=on")
                          : request_url;

  if (GetParam().initial_browser_content_filters_value) {
    // Google search is not on the exempt list of the URL Filter: search
    // requests must be explicitly allowed.
    EXPECT_CALL(*url_checker_client(),
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

  if (GetParam().initial_browser_content_filters_value) {
    EXPECT_CALL(*url_checker_client(),
                CheckURL(url_matcher::util::Normalize(request_url), _))
        .WillOnce([](const GURL& url,
                     URLCheckerClient::ClientCheckCallback callback) {
          std::move(callback).Run(url, ClientClassification::kAllowed);
        });
  } else {
    EXPECT_CALL(*url_checker_client(),
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
  if (!GetParam().initial_browser_content_filters_value) {
    GTEST_SKIP() << "This test requires the browser filter to be enabled.";
  }

  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  EXPECT_CALL(*url_checker_client(),
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
  if (GetParam().initial_browser_content_filters_value) {
    histogram_tester().ExpectBucketCount(
        "SupervisedUsers.WebFilterType.LocallySupervised",
        WebFilterType::kTryToBlockMatureSites, 1);
  } else if (GetParam().initial_search_content_filters_value) {
    histogram_tester().ExpectBucketCount(
        "SupervisedUsers.WebFilterType.LocallySupervised",
        WebFilterType::kDisabled, 1);
  } else {
    histogram_tester().ExpectTotalCount(
        "SupervisedUsers.WebFilterType.LocallySupervised", 0);
  }

  // This histogram is not recorded for locally supervised users.
  histogram_tester().ExpectTotalCount("FamilyUser.WebFilterType", 0);
}

IN_PROC_BROWSER_TEST_P(SupervisedUserServiceBootstrapAndroidBrowserTest,
                       FamilyLinkOverridesLocalSupervision) {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(GetProfile());
  bool is_initiall_supervised_locally =
      GetParam().initial_browser_content_filters_value ||
      GetParam().initial_search_content_filters_value;

  // Local supervision is initially enabled/disabled based on the test case, but
  // Family Link supervision is always disabled.
  ASSERT_EQ(service->IsSupervisedLocally(), is_initiall_supervised_locally);
  ASSERT_FALSE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));

  EnableParentalControls(*GetProfile()->GetPrefs());

  // Finally, local supervision is always disabled, Family Link supervision is
  // always enabled, and if there was a conflict, it's recorded.
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.FamilyLinkSupervisionConflict", 1,
      is_initiall_supervised_locally ? 1 : 0);
  EXPECT_FALSE(service->IsSupervisedLocally());
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
    testing::ValuesIn(kBootstrapServiceTestCases),
    [](const testing::TestParamInfo<BootstrapServiceTestCase>& info) {
      return info.param.test_name;
    });

// Tests the aspect where the Family Link supervision is enabled, but the
// content filters are not set.
class SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest
    : public SupervisedUserServiceBootstrapAndroidBrowserTestBase {
 protected:
  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls)
      override {
    return std::make_unique<FakeContentFiltersObserverBridge>(
        setting_name, on_enabled, on_disabled, is_subject_to_parental_controls,
        /*initial_value=*/false);
  }

  void SetUpPrefs(PrefService* local_state) override {
    EnableParentalControls(*local_state);
  }
};

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    IncognitoIsBlocked) {
  // TODO(http://crbug.com/433234589): this test could actually try to open
  // incognito (to no avail).
  EXPECT_EQ(static_cast<policy::IncognitoModeAvailability>(
                GetProfile()->GetPrefs()->GetInteger(
                    policy::policy_prefs::kIncognitoModeAvailability)),
            policy::IncognitoModeAvailability::kDisabled);
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    SafeSitesBlocksPages) {
  GURL request_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  EXPECT_CALL(*url_checker_client(),
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

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    WebFilterTypeIsRecordedOnce) {
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.WebFilterType.FamilyLink",
      WebFilterType::kTryToBlockMatureSites, 1);
  histogram_tester().ExpectBucketCount(
      "FamilyUser.WebFilterType", WebFilterType::kTryToBlockMatureSites, 1);
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserServiceBootstrapAndroidBrowserWithSupervisedUserTest,
    FamilyLinkIsImmuneToLocalSupervision) {
  TestSupervisedUserService* service = GetTestSupervisedUserService();

  // Local supervision is initially disabled and Family Link supervision is
  // initially enabled.
  ASSERT_FALSE(service->IsSupervisedLocally());
  ASSERT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));

  // Try turning the knob on the local supervision (browser filtering).
  service->browser_content_filters_observer_weak_ptr()->OnChange(
      /*env=*/nullptr,
      /*enabled=*/true);
  EXPECT_FALSE(service->IsSupervisedLocally());
  EXPECT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.FamilyLinkSupervisionConflict", 1, 1);

  // Try turning the knob on the local supervision (search filtering).
  service->search_content_filters_observer_weak_ptr()->OnChange(
      /*env=*/nullptr,
      /*enabled=*/true);
  EXPECT_FALSE(service->IsSupervisedLocally());
  EXPECT_TRUE(IsSubjectToParentalControls(*GetProfile()->GetPrefs()));
  histogram_tester().ExpectBucketCount(
      "SupervisedUsers.FamilyLinkSupervisionConflict", 1, 2);
}

// Tests the aspect where the Family Link supervision is disabled and the
// content filters are not set.
class SupervisedUserServiceBootstrapAndroidBrowserWithRegularUserTest
    : public SupervisedUserServiceBootstrapAndroidBrowserTestBase {
 protected:
  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls)
      override {
    return std::make_unique<FakeContentFiltersObserverBridge>(
        setting_name, on_enabled, on_disabled, is_subject_to_parental_controls,
        /*initial_value=*/false);
  }
};

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
  EXPECT_CALL(*url_checker_client(),
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

  EXPECT_CALL(*url_checker_client(),
              CheckURL(url_matcher::util::Normalize(url), _))
      .Times(0);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
}

}  // namespace
}  // namespace supervised_user
