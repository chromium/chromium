// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace apps {
namespace {

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

enum class NavCaptureAppType { kPwa, kIwa };

std::string NavCaptureTestParamToString(
    const testing::TestParamInfo<NavCaptureAppType>& params) {
  switch (params.param) {
    case NavCaptureAppType::kPwa:
      return "PWA";
    case NavCaptureAppType::kIwa:
      return "IWA";
  }
}
std::string OriginAssociationFileFromAppIdentity(std::string iwa_bundle_id,
                                                 std::string scope) {
  return *base::WriteJson(base::DictValue().Set(
      base::StringPrintf("isolated-app://%s/", iwa_bundle_id),
      base::DictValue().Set("scope", std::move(scope))));
}

// TODO(https://issues.chromium.org/329174385): Test more end-to-end the state
// of browser tabs rather than relying on testing callbacks, this requires
// resolving flakiness with awaiting web app launches.
class LinkCapturingNavigationThrottleBrowserTest
    : public web_app::WebAppNavigationCapturingBrowserTestBase,
      public testing::WithParamInterface<NavCaptureAppType> {
 public:
  LinkCapturingNavigationThrottleBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps,
#if !BUILDFLAG(IS_CHROMEOS)
         features::kIsolatedWebApps
#endif  // !BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  bool UseIwa() { return GetParam() == NavCaptureAppType::kIwa; }

  void SetFakeOriginAssociationFetcher(
      url::Origin request_origin,
      const web_package::SignedWebBundleId& bundle_id,
      std::string scope) {
    auto origin_association_fetcher =
        std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();

    origin_association_fetcher->SetData(
        {{std::move(request_origin), OriginAssociationFileFromAppIdentity(
                                         bundle_id.id(), std::move(scope))}});

    web_app::WebAppProvider::GetForWebApps(GetProfile())
        ->origin_association_manager()
        .SetFetcherForTest(std::move(origin_association_fetcher));
  }

  webapps::AppId InstallIwa() {
    const url::Origin scope_extended_origin = url::Origin::Create(start_url_);
    // Only capture start_url_ to prevent issues when capturing irrelevant urls.
    SetFakeOriginAssociationFetcher(
        scope_extended_origin,
        web_package::test::GetDefaultEd25519WebBundleId(),
        start_url_.GetPath());

    web_app::IsolatedWebAppUrlInfo url_info =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().AddScopeExtension(
                scope_extended_origin,
                /*has_origin_wildcard=*/false))
            .BuildBundle(web_package::test::GetDefaultEd25519KeyPair())
            ->InstallChecked(browser()->profile());

    return url_info.app_id();
  }

  webapps::AppId InstallPwa() {
    return web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(),
                                                            start_url_);
  }

  void SetUpOnMainThread() override {
#if (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
    // TODO(https://issues.chromium.org/329174385): Deflake tests in
    // Linux MSan Tests / Mac Tests / Win ASan Tests.
    GTEST_SKIP();
#else
    web_app::WebAppNavigationCapturingBrowserTestBase::SetUpOnMainThread();

    // Set up web app as link capturing target.
    start_url_ = https_server()->GetURL("/web_apps/basic.html");
    // Set up simple page browser tab with a "fill-page" CSS class available for
    // tests to use. This class makes elements fill the page making it easy to
    // click on.
    simple_url_ = https_server()->GetURL("/simple.html");
    ASSERT_TRUE(content::NavigateToURL(GetBrowserTab(), simple_url_));
    ASSERT_TRUE(content::ExecJs(GetBrowserTab(), R"(
      const style = document.createElement('style');
      style.textContent = `
        .fill-page {
          position: absolute;
          left: 0;
          top: 0;
          width: 100vw;
          height: 100vh;
        }
      `;
      document.head.append(style);
    )"));

    // Set up web app as link capturing target.
    start_url_ = https_server()->GetURL("/web_apps/basic.html");
    switch (GetParam()) {
      case NavCaptureAppType::kPwa:
        app_id_ = InstallPwa();
        apps::AppReadinessWaiter(browser()->profile(), app_id_).Await();
        break;
      case NavCaptureAppType::kIwa:
        app_id_ = InstallIwa();
        break;
    }

    ASSERT_TRUE(test::EnableLinkCapturingByUser(browser()->profile(), app_id_)
                    .has_value());
#endif
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  content::WebContents* GetBrowserTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void ClickPage() {
    content::SimulateMouseClick(GetBrowserTab(), /*modifiers=*/0,
                                blink::WebMouseEvent::Button::kLeft);
  }

  content::WebContents* ClickPageAndWaitForStartUrlLoad() {
    test::NavigationCommittedForUrlObserver load_observer(start_url_);
    ClickPage();
    load_observer.Wait();
    return load_observer.web_contents();
  }

  AssertionResult ClickLinkExpectNewAppWindow() {
    ui_test_utils::BrowserCreatedObserver browser_observer;

    ClickPage();

    Browser* app_browser = browser_observer.Wait();
    if (app_id_ != app_browser->app_controller()->app_id()) {
      return AssertionFailure()
             << "Started browser has app_id=" << app_id_
             << ", but expected=" << app_browser->app_controller()->app_id();
    }

    return AssertionSuccess();
  }

  AssertionResult ClickLinkExpectBrowserTab() {
    content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
    if (!loaded_contents) {
      return AssertionFailure() << "Navigated to null web contents";
    }

    bool loaded_in_app_window =
        web_app::WebAppTabHelper::FromWebContents(loaded_contents)
            ->is_in_app_window();

    if (loaded_in_app_window) {
      return AssertionFailure() << "Expected to load url not in app window";
    }

    return AssertionSuccess();
  }

 protected:
  GURL simple_url_;
  GURL start_url_;
  webapps::AppId app_id_;
  base::test::ScopedFeatureList feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClick) {
  // Insert an <a href> link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlank) {
  // Insert an <a href target="_blank"> link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectNewAppWindow());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickServerRedirect) {
  // Insert an <a href> server redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        document.body.append(link);
      )",
      https_server()
          ->GetURL("/server-redirect?" + start_url_.spec())
          .spec()
          .c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlankServerRedirect) {
  // Insert an <a href target="_blank"> server redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      https_server()
          ->GetURL("/server-redirect?" + start_url_.spec())
          .spec()
          .c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectNewAppWindow());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlankJavaScriptRedirect) {
  // Insert an <a href target="_blank"> JavaScript redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '/navigate_to.html?%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByButtonClickJavaScriptWindowOpen) {
  // Insert a window.open(url) button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          window.open('%s');
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

IN_PROC_BROWSER_TEST_P(
    LinkCapturingNavigationThrottleBrowserTest,
    TriggeredByButtonClickJavaScriptWindowOpenAboutBlankLocationHrefUrl) {
  // Insert a window.open('about:blank').location.href = url button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          window.open('about:blank').location.href = '%s';
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByButtonClickJavaScriptLocationHrefUrl) {
  // Insert a location.href = url button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          location.href = '%s';
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  ASSERT_TRUE(ClickLinkExpectBrowserTab());
}

// TODO(crbug.com/405508964): Test is flaky.
IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       DISABLED_TriggeredByNoClickJavaScriptWindowOpen) {
  ui_test_utils::UrlLoadObserver load_observer(start_url_);
  ASSERT_TRUE(content::ExecJs(
      GetBrowserTab(),
      base::StringPrintf("window.open('%s')", start_url_.spec().c_str())));
  load_observer.Wait();
  ASSERT_TRUE(load_observer.web_contents());

  EXPECT_FALSE(
      web_app::WebAppTabHelper::FromWebContents(load_observer.web_contents())
          ->is_in_app_window());
}

INSTANTIATE_TEST_SUITE_P(,
                         LinkCapturingNavigationThrottleBrowserTest,
                         testing::Values(NavCaptureAppType::kPwa,
                                         NavCaptureAppType::kIwa),
                         NavCaptureTestParamToString);

}  // namespace
}  // namespace apps
