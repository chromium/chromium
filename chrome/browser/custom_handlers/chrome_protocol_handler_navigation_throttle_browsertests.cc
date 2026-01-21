// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/chrome_protocol_handler_registry_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_navigation_throttle.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

using HandlerPermissionGrantedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionGrantedCallback;
using HandlerPermissionDeniedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionDeniedCallback;

class CustomProtocolHandlerRegistryDelegate
    : public ChromeProtocolHandlerRegistryDelegate {
  void RegisterWithOSAsDefaultClient(const std::string& protocol,
                                     DefaultClientCallback callback) override {}
  bool ShouldRemoveHandlersNotInOS() override { return false; }
};

class ChromeProtocolHandlerNavigationThrottleBrowserBaseTest
    : public InProcessBrowserTest {
 public:
  ChromeProtocolHandlerNavigationThrottleBrowserBaseTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/");
    ASSERT_TRUE(embedded_test_server()->Start());

    // The OS registration of a handler as default may fails in Testing
    // environments for a variety of reasons; for instance, in mac the
    // app_bundle got in SetAsDefaultClientForScheme() is not valid. Given
    // the result of ShouldRemoveHandlersNotInOS()
    // the handler will be removed in OnSetAsDefaultProtocolClientFinished()
    // when the handler registration procedure completes. Linux and ChromeOS
    // don't currently hit this problem as their implementations of
    // ShouldRemoveHandlersNotInOS() always return false, but we may want to
    // provide a real implementation in the future. Since the OS registration
    // is not a fundamental part of the feature being tested here, it's better
    // to disable it completely for any platform.
    registry()->SetDelegateForTesting(
        std::make_unique<CustomProtocolHandlerRegistryDelegate>());
  }

  void TearDownOnMainThread() override {
    custom_handlers::ProtocolHandlerNavigationThrottle::
        GetDialogLaunchCallbackForTesting() = custom_handlers::
            ProtocolHandlerNavigationThrottle::LaunchCallbackForTesting();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  custom_handlers::ProtocolHandlerRegistry* registry() {
    return ProtocolHandlerRegistryFactory::GetForBrowserContext(
        browser()->profile());
  }
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  custom_handlers::ProtocolHandler CreateUnconfirmedProtocolHandler(
      const std::string& protocol,
      const GURL& url) {
    // Only handlers with extension id can be unconfirmed for now.
    return custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
        protocol, url, kTestExtensionId);
  }

  void RegisterProtocolHandler(const std::string& scheme,
                               const GURL& handler_url) {
    custom_handlers::ProtocolHandler ph1 =
        CreateUnconfirmedProtocolHandler(scheme, handler_url);
    registry()->OnAcceptRegisterProtocolHandler(ph1);
    ASSERT_TRUE(registry()->IsHandledProtocol(scheme));
    ASSERT_FALSE(registry()->IsProtocolHandlerConfirmed(scheme));
  }

  void SetConfirmTestingCallback(bool permission_granted, bool remember) {
    custom_handlers::ProtocolHandlerNavigationThrottle::
        GetDialogLaunchCallbackForTesting() = base::BindRepeating(
            [](bool accept, bool save,
               HandlerPermissionGrantedCallback granted_callback,
               HandlerPermissionDeniedCallback denied_callback) {
              if (accept) {
                std::move(granted_callback).Run(save);
              } else {
                std::move(denied_callback).Run();
              }
            },
            permission_granted, remember);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class ChromeProtocolHandlerNavigationThrottleBrowserTest
    : public ChromeProtocolHandlerNavigationThrottleBrowserBaseTest {
 public:
  ChromeProtocolHandlerNavigationThrottleBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionProtocolHandlers);
  }
};

// Grant permission to use a custom handler allows the redirection of a
// navigation request to an URL with an unknown scheme. This decision is
// saved in the memory (BrowserContext), although it's not stored in the
// profile settings.
IN_PROC_BROWSER_TEST_F(ChromeProtocolHandlerNavigationThrottleBrowserTest,
                       AcceptFirstThenDeny) {
  // Register protocol handler.
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Grant permission, but don't save the changes in the preferences store.
  SetConfirmTestingCallback(/*permission_granted=*/true, /*remember=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:test")));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());

  // Simulate a new BrowserContext creation by clearing the
  // ProtocolHandlerRegistry in memory and load the handlers stored in the
  // profile settings.
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max(), false);
  registry()->InitProtocolSettings();

  // Initial navigation to check against, given that the new navigation won't be
  // completed.
  GURL initial_url =
      embedded_test_server()->GetURL("/custom_handlers/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Deny permission now.
  SetConfirmTestingCallback(/*permission_granted=*/false, /*remember=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:test")));
  ASSERT_EQ(initial_url, web_contents()->GetLastCommittedURL());
}

// User grants permission and choses to remember the decision, so the custom
// handler is confirmed and its new state saved in the user's preferences.
// Hence, the next navigation requests uses the registered protocol handler
// independently of the values passed to the confirmation testing callback.
IN_PROC_BROWSER_TEST_F(ChromeProtocolHandlerNavigationThrottleBrowserTest,
                       AcceptAndSaveFirstThenDeny) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Grant permission and save the changes in the preferences store.
  SetConfirmTestingCallback(/*permission_granted*/ true, /*remember*/ true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:test")));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());

  // Simulate a new BrowserContext creation by clearing the
  // ProtocolHandlerRegistry in memory and load the handlers stored in the
  // profile settings.
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max(), false);
  registry()->InitProtocolSettings();

  // Initial navigation to check against, to ensure denying the use of the
  // handler doesn't take any effect.
  GURL initial_url =
      embedded_test_server()->GetURL("/custom_handlers/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Deny permission now, but the user chose to remember the previous decision,
  // hence the handler is confirmed now and the user won't be prompted again.
  SetConfirmTestingCallback(/*permission_granted=*/false, /*remember=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:test")));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
}

class ChromeProtocolHandlerNavigationThrottleBrowserParamTest
    : public ChromeProtocolHandlerNavigationThrottleBrowserBaseTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeProtocolHandlerNavigationThrottleBrowserParamTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          extensions_features::kExtensionProtocolHandlers);
    } else {
      feature_list_.InitAndDisableFeature(
          extensions_features::kExtensionProtocolHandlers);
    }
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WithExtensionProtocolHandlers"
                      : "WithoutExtensionProtocolHandlers";
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeProtocolHandlerNavigationThrottleBrowserParamTest,
    ::testing::Bool(),
    ChromeProtocolHandlerNavigationThrottleBrowserParamTest::DescribeParams);

// If the ExtensionProtocolHandlers feature is not enabled the
// ProtocolHandlerThrottle is not allowed to handle the unknown scheme and
// translate the URL so that the navigation request could be completed.
IN_PROC_BROWSER_TEST_P(ChromeProtocolHandlerNavigationThrottleBrowserParamTest,
                       CheckFeatureFlag) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Grant permission to use the handler.
  SetConfirmTestingCallback(/*permission_granted=*/true, /*remember=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  if (GetParam()) {
    ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
  } else {
    // Since there is NavigationThrottle the protocol handler is still
    // unconfirmed.
    ASSERT_EQ(GURL("about:blank"), web_contents()->GetLastCommittedURL());
  }
}
