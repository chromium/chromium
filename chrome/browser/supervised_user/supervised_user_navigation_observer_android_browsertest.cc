// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

namespace supervised_user {
namespace {

// Covers extra behaviors available only in Clank (Android). See supervised user
// navigation and throttle tests for general behavior.
class SupervisedUserNavigationObserverAndroidBrowserTest
    : public AndroidBrowserTest {
 protected:
  // Create a new tab (about:blank). The most recently added tab constitutes the
  // current web contents of this test fixture.
  void AddTab() {
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    TabAndroid* new_tab = TabAndroid::FromWebContents(web_contents());
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(content::WebContents::CreateParams(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
    content::WebContents* new_web_contents = contents.release();
    content::NavigationController::LoadURLParams params(GURL("about:blank"));
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);
    params.has_user_gesture = true;
    new_web_contents->GetController().LoadURLWithParams(params);
    tab_model->CreateTab(new_tab, new_web_contents, /*select=*/true);
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  FakeContentFiltersObserverBridge* search_content_filters_observer() {
    return search_content_filters_observer_.get();
  }

 private:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    // TODO(crbug.com/426773953): Set testing factory takes browser context
    // before its substitution, meaning that services are already created and
    // attached to the navigation in the default tab. Replacing the factory will
    // yield new services but the navigation observer will still refer to the
    // old service, so all current tabs are of no use in the context of this
    // test.
    SupervisedUserServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        web_contents()->GetBrowserContext(),
        base::BindRepeating(
            &SupervisedUserNavigationObserverAndroidBrowserTest::
                BuildSupervisedUserService,
            base::Unretained(this)));

    // Will resolve google.com to localhost, so the embedded test server can
    // serve a valid content for it.
    host_resolver()->AddRule("google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL().path() != "/search") {
            return nullptr;
          }
          // HTTP 200 OK with empty response body.
          return std::make_unique<net::test_server::BasicHttpResponse>();
        }));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpCommandLine(command_line);
    // The production code only allows known ports (80 for http and 443 for
    // https), but the embedded test server runs on a random port and adds it to
    // the url spec.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  // Builds a SupervisedUserService with a fake content filters observer bridge
  // exposed for testing.
  std::unique_ptr<KeyedService> BuildSupervisedUserService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);

    std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);

    return std::make_unique<SupervisedUserService>(
        IdentityManagerFactory::GetForProfile(profile),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        *profile->GetPrefs(),
        *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()),
        SyncServiceFactory::GetInstance()->GetForProfile(profile),
        std::make_unique<SupervisedUserURLFilter>(
            *profile->GetPrefs(), std::make_unique<FakeURLFilterDelegate>(),
            std::make_unique<KidsChromeManagementURLCheckerClient>(
                IdentityManagerFactory::GetForProfile(profile),
                profile->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess(),
                *profile->GetPrefs(), platform_delegate->GetCountryCode(),
                platform_delegate->GetChannel())),
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
        base::BindRepeating(
            &SupervisedUserNavigationObserverAndroidBrowserTest::CreateBridge,
            base::Unretained(this)));
  }

  // Creates a fake content filters observer bridge for testing, and binds it to
  // this test fixture.
  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled) {
    std::unique_ptr<FakeContentFiltersObserverBridge> bridge =
        std::make_unique<FakeContentFiltersObserverBridge>(
            setting_name, on_enabled, on_disabled);
    if (setting_name == kSearchContentFiltersSettingName) {
      search_content_filters_observer_ = bridge.get();
    }
    return bridge;
  }

  raw_ptr<FakeContentFiltersObserverBridge> search_content_filters_observer_;
  base::test::ScopedFeatureList scoped_feature_list_{
      kPropagateDeviceContentFiltersToSupervisedUser};
};

// With disabled search content filters, the navigation is unchanged and safe
// search query params are not appended.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationObserverAndroidBrowserTest,
                       DontPropagateSearchContentFilterSettingWhenDisabled) {
  ASSERT_FALSE(search_content_filters_observer()->IsEnabled());

  // The loaded URL is exactly as requested.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("google.com", "/search?q=cat")));
}

// Verifies that the search content filter setting is propagated through the
// supervised user service to navigation throttles that alter the URL. This
// particular test doesn't require navigation observer, but is hosted here for
// feature consistency.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationObserverAndroidBrowserTest,
                       LoadSafeSearchResultsWithSearchContentFilterPreset) {
  search_content_filters_observer()->SetEnabled(true);
  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");

  // The final url will be different: with safe search query params.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), url, GURL(url.spec() + "&safe=active&ssui=on")));
}

// Similar to the above test, but the URL already contains safe search query
// params (for example, from a previous navigation or added manually by user in
// the Omnibox). They are removed regardless of their value, and safe search
// params are appended.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationObserverAndroidBrowserTest,
                       PreexistingSafeSearchParamsAreRemovedBeforeAppending) {
  search_content_filters_observer()->SetEnabled(true);
  GURL url = embedded_test_server()->GetURL("google.com",
                                            "/search?safe=off&ssui=on&q=cat");

  // The final url will be different: with extra query params appended and
  // previous ones removed.
  GURL expected_url = embedded_test_server()->GetURL(
      "google.com", "/search?q=cat&safe=active&ssui=on");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url, expected_url));
}

// Verifies that the search content filter is propagated through the supervised
// user service to to the navigation observer, and that the navigation observer
// triggers the page reload.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationObserverAndroidBrowserTest,
                       ReloadSearchResultAfterSearchContentFilterIsEnabled) {
  // Creating new tab will bootstrap it with the navigation observer with a
  // supervised user service from the replaced factory. It becomes the current
  // tab and web contents.
  AddTab();

  // Verify that the observer is attached.
  ASSERT_NE(SupervisedUserNavigationObserver::FromWebContents(web_contents()),
            nullptr);

  GURL url = embedded_test_server()->GetURL("google.com", "/search?q=cat");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  content::TestNavigationObserver navigation_observer(web_contents());
  search_content_filters_observer()->SetEnabled(true);
  navigation_observer.Wait();

  // Key part: the search results are reloaded with extra query params.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(),
            url.spec() + "&safe=active&ssui=on");
}

}  // namespace
}  // namespace supervised_user
