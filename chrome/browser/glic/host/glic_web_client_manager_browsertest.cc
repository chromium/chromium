// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class TestSecurityInterstitialControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  TestSecurityInterstitialControllerClient(content::WebContents* web_contents,
                                           const GURL& request_url,
                                           base::RepeatingClosure on_proceed)
      : SecurityInterstitialControllerClient(
            web_contents,
            CreateMetricsHelper(request_url),
            /*prefs=*/nullptr,
            base::i18n::GetConfiguredLocale(),
            /*default_safe_page=*/GURL("about:blank"),
            /*settings_page_helper=*/nullptr),
        on_proceed_(std::move(on_proceed)) {}

  void Proceed() override {
    if (on_proceed_) {
      on_proceed_.Run();
    }
    Reload();
  }

  void GoBack() override { GoBackAfterNavigationCommitted(); }

 private:
  static std::unique_ptr<security_interstitials::MetricsHelper>
  CreateMetricsHelper(const GURL& url) {
    security_interstitials::MetricsHelper::ReportDetails settings;
    settings.metric_prefix = "test_enterprise_warn";
    return std::make_unique<security_interstitials::MetricsHelper>(
        url, settings, nullptr);
  }

  base::RepeatingClosure on_proceed_;
};

class TestSecurityInterstitialPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  static constexpr SecurityInterstitialPage::TypeID kTypeForTesting =
      &kTypeForTesting;

  TestSecurityInterstitialPage(content::WebContents* web_contents,
                               const GURL& request_url,
                               base::RepeatingClosure on_proceed)
      : SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<TestSecurityInterstitialControllerClient>(
                web_contents,
                request_url,
                std::move(on_proceed))) {}

  ~TestSecurityInterstitialPage() override = default;

  SecurityInterstitialPage::TypeID GetTypeForTesting() override {
    return kTypeForTesting;
  }

  std::string GetHTMLContents() override {
    return "<html><body>Enterprise Warning Interstitial</body></html>";
  }

  void OnInterstitialClosing() override {}

  void CommandReceived(const std::string& command) override {
    if (command == base::NumberToString(security_interstitials::CMD_PROCEED)) {
      controller()->Proceed();
    } else if (command ==
               base::NumberToString(security_interstitials::CMD_DONT_PROCEED)) {
      controller()->GoBack();
    }
  }

 protected:
  void PopulateInterstitialStrings(base::DictValue& load_time_data) override {}
};

class SecurityInterstitialNavigationThrottle
    : public content::NavigationThrottle {
 public:
  SecurityInterstitialNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      base::RepeatingClosure on_proceed)
      : content::NavigationThrottle(registry),
        on_proceed_(std::move(on_proceed)) {}

  const char* GetNameForLogging() override {
    return "SecurityInterstitialNavigationThrottle";
  }

  ThrottleCheckResult WillProcessResponse() override {
    content::NavigationHandle* handle = navigation_handle();
    auto blocking_page = std::make_unique<TestSecurityInterstitialPage>(
        handle->GetWebContents(), handle->GetURL(), on_proceed_);
    std::string html = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, std::move(blocking_page));
    return ThrottleCheckResult(CANCEL, net::ERR_CERT_COMMON_NAME_INVALID, html);
  }

 private:
  base::RepeatingClosure on_proceed_;
};

class NetworkErrorNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit NetworkErrorNavigationThrottle(
      content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  const char* GetNameForLogging() override {
    return "NetworkErrorNavigationThrottle";
  }

  ThrottleCheckResult WillStartRequest() override {
    return ThrottleCheckResult(CANCEL, net::ERR_INTERNET_DISCONNECTED);
  }
};

class GlicWebClientManagerBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  GlicWebClientManagerBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kGlicWebContentsWarming,
         {{features::kGlicWebContentsWarmingDelay.name, "7d"}}},
        {features::kGlicWebClientLoadTimes,
         {{features::kGlicMaxLoadingTimeMs.name, "60000"}}},
    };
    std::vector<base::test::FeatureRef> disabled;

    if (IsNoWebview()) {
      enabled.push_back({features::kGlicNoWebview, {}});
    } else {
      disabled.push_back(features::kGlicNoWebview);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/_up") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content("<html><body>Login</body></html>");
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));
    GlicBrowserTestMixin<PlatformBrowserTest>::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    creation_subscription_ = {};
    inserters_.clear();
    GlicBrowserTestMixin<PlatformBrowserTest>::TearDownOnMainThread();
  }

  bool IsNoWebview() const { return GetParam(); }

  bool IsGuestUrl(const GURL& url) {
    const GURL base_guest_url = GetGuestURL();
    return url.host() == base_guest_url.host() &&
           url.port() == base_guest_url.port() &&
           url.path() == base_guest_url.path();
  }

  void WaitForInterstitialDisplayed(GlicInstanceImpl* instance) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      content::WebContents* guest_contents =
          instance->host().web_client_contents();
      if (!guest_contents) {
        return false;
      }
      auto* helper = security_interstitials::SecurityInterstitialTabHelper::
          FromWebContents(guest_contents);
      return helper && helper->IsDisplayingInterstitial();
    }));

    if (IsNoWebview()) {
      ASSERT_OK(WaitForGuestState(
          GlicNoWebviewContentsManager::GuestState::kSecurityInterstitial,
          instance));
      ASSERT_OK(WaitForDisplayState(
          GlicNoWebviewContentsManager::DisplayState::kShowingGuest, instance));
    } else {
      ASSERT_TRUE(base::test::RunUntil(
          [&]() { return instance->host().webui_contents() != nullptr; }));
      content::WebContents* webui_contents = instance->host().webui_contents();
      ASSERT_TRUE(content::WaitForLoadStop(webui_contents));
      constexpr char kWaitForGuestPanelVisibleScript[] = R"js(
        new Promise((resolve) => {
          const check = () => {
            const errorPanel = document.getElementById('errorPanel');
            if (errorPanel && !errorPanel.hidden) {
              resolve('errorPanel became visible while waiting for guestPanel');
              return;
            }
            const panel = document.getElementById('guestPanel');
            if (panel && !panel.hidden &&
                panel.classList.contains('show-header')) {
              resolve(true);
              return;
            }
            setTimeout(check, 10);
          };
          check();
        })
      )js";
      ASSERT_EQ(true, content::EvalJs(webui_contents,
                                      kWaitForGuestPanelVisibleScript));
      ASSERT_NE(instance->host().GetPrimaryWebUiState(),
                mojom::WebUiState::kError);
    }
  }

  void SendInterstitialCommand(
      content::WebContents* guest_contents,
      security_interstitials::SecurityInterstitialCommand command) {
    ASSERT_TRUE(guest_contents);
    auto* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            guest_contents);
    ASSERT_TRUE(helper);
    security_interstitials::SecurityInterstitialPage* interstitial =
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
    ASSERT_TRUE(interstitial);
    interstitial->CommandReceived(base::NumberToString(command));
  }

 protected:
  bool block_with_interstitial_ = true;
  bool fail_with_network_error_ = false;
  std::vector<std::unique_ptr<content::TestNavigationThrottleInserter>>
      inserters_;
  base::CallbackListSubscription creation_subscription_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedRecordedOnClientLoad) {
  base::HistogramTester histogram_tester;

  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);
  auto samples = histogram_tester.GetAllSamples("Glic.Contents.TimeToWarmed");
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_GT(samples[0].min, 0);
}

// TODO(crbug.com/572980728): Flaky on all platforms, and consistently
// failing on some builders (e.g. Win10 Tests x64, mac-fieldtrial-tester).
IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       DISABLED_TimeToWarmedRecordedOnceOnly) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);

  // Subsequent open calls for another tab do not re-record TimeToWarmed for
  // the existing web client.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK(OpenGlicForActiveTab());
  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);

  // Subsequent state event (kWarmed) or receiver creation on the manager does
  // not re-record TimeToWarmed because it has already been recorded.
  GlicWebClientManager* manager =
      instance->host().GetWebClientManagerForTesting();
  ASSERT_TRUE(manager);
  manager->OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);

  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager->SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedNotRecordedOnNonRegularNavigation) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::WebContents> test_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  GlicWebClientManager manager;
  manager.AttachGuestContents(test_contents.get());

  // Navigating to a login URL (e.g. /_up) classifies the page
  // as non-regular (kLogin), disqualifying it from TimeToWarmed recording.
  ASSERT_TRUE(content::NavigateToURL(test_contents.get(),
                                     embedded_test_server()->GetURL("/_up")));

  manager.OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);
  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager.SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 0);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedNotRecordedOnCrash) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::WebContents> test_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  GlicWebClientManager manager;
  manager.AttachGuestContents(test_contents.get());

  // Navigate to an initial regular page so a render process host exists.
  ASSERT_TRUE(content::NavigateToURL(
      test_contents.get(), embedded_test_server()->GetURL("/title1.html")));

  // A render process crash disqualifies the contents from TimeToWarmed
  // recording.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::CrashTab(test_contents.get());
  }

  manager.OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);
  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager.SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 0);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       RecordsWebClientCrashExitReason) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::WebContents> test_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  GlicWebClientManager manager;
  manager.AttachGuestContents(test_contents.get());

  ASSERT_TRUE(content::NavigateToURL(
      test_contents.get(), embedded_test_server()->GetURL("/title1.html")));

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::CrashTab(test_contents.get());
  }

  // The exact exit reason depends on the platform, so only check the count.
  histogram_tester.ExpectTotalCount("Glic.Contents.WebClientCrash.ExitReason",
                                    1);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       SecurityInterstitialShownAndProceedLoadsClient) {
  block_with_interstitial_ = true;

  creation_subscription_ = content::RegisterWebContentsCreationCallback(
      base::BindLambdaForTesting([this](content::WebContents* contents) {
        inserters_.push_back(
            std::make_unique<content::TestNavigationThrottleInserter>(
                contents,
                base::BindLambdaForTesting(
                    [this](content::NavigationThrottleRegistry& registry) {
                      content::NavigationHandle& handle =
                          registry.GetNavigationHandle();
                      if (block_with_interstitial_ &&
                          handle.IsInPrimaryMainFrame() &&
                          IsGuestUrl(handle.GetURL())) {
                        registry.AddThrottle(
                            std::make_unique<
                                SecurityInterstitialNavigationThrottle>(
                                registry, base::BindLambdaForTesting([this]() {
                                  block_with_interstitial_ = false;
                                })));
                      }
                    })));
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_NO_FATAL_FAILURE(WaitForInterstitialDisplayed(instance));

  // Simulate clicking "Proceed" on the security interstitial, which allowlists
  // the navigation and reloads in the same guest WebContents.
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_NO_FATAL_FAILURE(SendInterstitialCommand(
      guest_contents, security_interstitials::CMD_PROCEED));

  // Verify the reload in the same guest WebContents succeeds and initializes
  // the Glic web client.
  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_EQ(instance->host().web_client_contents(), guest_contents);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       SecurityInterstitialDontProceedTransitionsToError) {
  creation_subscription_ = content::RegisterWebContentsCreationCallback(
      base::BindLambdaForTesting([this](content::WebContents* contents) {
        inserters_.push_back(
            std::make_unique<content::TestNavigationThrottleInserter>(
                contents,
                base::BindLambdaForTesting(
                    [this](content::NavigationThrottleRegistry& registry) {
                      content::NavigationHandle& handle =
                          registry.GetNavigationHandle();
                      if (handle.IsInPrimaryMainFrame() &&
                          IsGuestUrl(handle.GetURL())) {
                        registry.AddThrottle(
                            std::make_unique<
                                SecurityInterstitialNavigationThrottle>(
                                registry, base::DoNothing()));
                      }
                    })));
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_NO_FATAL_FAILURE(WaitForInterstitialDisplayed(instance));

  // Simulate clicking "Go back" (CMD_DONT_PROCEED) on the interstitial.
  // The panel should transition directly to the error state without hanging on
  // the loading skeleton.
  ASSERT_NO_FATAL_FAILURE(
      SendInterstitialCommand(instance->host().web_client_contents(),
                              security_interstitials::CMD_DONT_PROCEED));

  if (IsNoWebview()) {
    ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kError, instance));
    ASSERT_OK(WaitForDisplayState(
        GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));
  } else {
    ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
  }
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       PostCommitSecurityInterstitialShownAndProceedRecovers) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  const GURL guest_url = guest_contents->GetLastCommittedURL();

  // Simulate a SecurityInterstitialNavigationThrottle blocking a post-commit
  // guest navigation (e.g. after initial client load).
  block_with_interstitial_ = true;
  inserters_.push_back(
      std::make_unique<content::TestNavigationThrottleInserter>(
          guest_contents,
          base::BindLambdaForTesting(
              [this](content::NavigationThrottleRegistry& registry) {
                content::NavigationHandle& handle =
                    registry.GetNavigationHandle();
                if (block_with_interstitial_ && handle.IsInPrimaryMainFrame() &&
                    IsGuestUrl(handle.GetURL())) {
                  registry.AddThrottle(
                      std::make_unique<SecurityInterstitialNavigationThrottle>(
                          registry, base::BindLambdaForTesting([this]() {
                            block_with_interstitial_ = false;
                          })));
                }
              })));

  guest_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  ASSERT_NO_FATAL_FAILURE(WaitForInterstitialDisplayed(instance));
  ASSERT_NO_FATAL_FAILURE(SendInterstitialCommand(
      guest_contents, security_interstitials::CMD_PROCEED));

  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_EQ(instance->host().web_client_contents(), guest_contents);

  if (IsNoWebview()) {
    // Also verify LoadPostCommitErrorPage (used by real-time enterprise URL
    // checks in NoWebview / PrivilegedWebContents mode where
    // BrowserPluginGuest does not suppress ERR_BLOCKED_BY_CLIENT).
    auto blocking_page = std::make_unique<TestSecurityInterstitialPage>(
        guest_contents, guest_url, base::DoNothing());
    std::string html = blocking_page->GetHTMLContents();
    base::WeakPtr<content::NavigationHandle> handle =
        guest_contents->GetController().LoadPostCommitErrorPage(
            guest_contents->GetPrimaryMainFrame(), guest_url, html);
    ASSERT_TRUE(handle);
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle.get(), std::move(blocking_page));

    ASSERT_NO_FATAL_FAILURE(WaitForInterstitialDisplayed(instance));
    ASSERT_NO_FATAL_FAILURE(SendInterstitialCommand(
        guest_contents, security_interstitials::CMD_PROCEED));

    ASSERT_OK(WaitForGlicClient(instance));
    EXPECT_EQ(instance->host().web_client_contents(), guest_contents);
  }
}

IN_PROC_BROWSER_TEST_P(
    GlicWebClientManagerBrowserTest,
    SecurityInterstitialFollowedByNetworkErrorTransitionsToError) {
  block_with_interstitial_ = true;
  fail_with_network_error_ = false;

  creation_subscription_ = content::RegisterWebContentsCreationCallback(
      base::BindLambdaForTesting([this](content::WebContents* contents) {
        inserters_.push_back(
            std::make_unique<content::TestNavigationThrottleInserter>(
                contents,
                base::BindLambdaForTesting(
                    [this](content::NavigationThrottleRegistry& registry) {
                      content::NavigationHandle& handle =
                          registry.GetNavigationHandle();
                      if (!handle.IsInPrimaryMainFrame() ||
                          !IsGuestUrl(handle.GetURL())) {
                        return;
                      }
                      if (block_with_interstitial_) {
                        registry.AddThrottle(
                            std::make_unique<
                                SecurityInterstitialNavigationThrottle>(
                                registry, base::BindLambdaForTesting([this]() {
                                  block_with_interstitial_ = false;
                                  fail_with_network_error_ = true;
                                })));
                      } else if (fail_with_network_error_) {
                        registry.AddThrottle(
                            std::make_unique<NetworkErrorNavigationThrottle>(
                                registry));
                      }
                    })));
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_NO_FATAL_FAILURE(WaitForInterstitialDisplayed(instance));

  // Clicking "Proceed" reloads, and the reload fails with a non-interstitial
  // network error. Verify it is classified as kLoadError (not
  // kSecurityInterstitial) and transitions to the error state.
  ASSERT_NO_FATAL_FAILURE(
      SendInterstitialCommand(instance->host().web_client_contents(),
                              security_interstitials::CMD_PROCEED));

  if (IsNoWebview()) {
    ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kError, instance));
    ASSERT_OK(WaitForDisplayState(
        GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));
  } else {
    ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicWebClientManagerBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace
}  // namespace glic
