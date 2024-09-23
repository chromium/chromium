// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class ReferrerPolicyTest : public InProcessBrowserTest {
 public:
  ReferrerPolicyTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Subclasses might want to verify the requests that arrive at servers;
    // register a default no-op handler that subclasses may override.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ReferrerPolicyTest::OnServerIncomingRequest, base::Unretained(this)));
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &ReferrerPolicyTest::OnServerIncomingRequest, base::Unretained(this)));

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(https_server_.Start());
  }
  ~ReferrerPolicyTest() override = default;

  enum ExpectedReferrer {
    EXPECT_EMPTY_REFERRER,
    EXPECT_FULL_REFERRER,
    EXPECT_ORIGIN_AS_REFERRER
  };

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some builders are flaky due to slower loading interacting
    // with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

 protected:
  // Callback to verify that HTTP requests have the correct headers;
  // currently, (See the comment on RequestCheck, below.)
  virtual void OnServerIncomingRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock lock(check_on_requests_lock_);
    if (!check_on_requests_)
      return;

    if (request.relative_url != check_on_requests_->destination_url_to_match)
      return;

    auto it = request.headers.find("Referer");

    if (check_on_requests_->expected_spec.empty()) {
      EXPECT_TRUE(it == request.headers.end()) << it->second;
    } else {
      EXPECT_TRUE(it != request.headers.end());
      if (it != request.headers.end())
        EXPECT_EQ(it->second, check_on_requests_->expected_spec);
    }
  }

  // Returns the expected title for the tab with the given (full) referrer and
  // the expected modification of it.
  std::u16string GetExpectedTitle(const GURL& url,
                                  ExpectedReferrer expected_referrer) {
    std::string referrer;
    switch (expected_referrer) {
      case EXPECT_EMPTY_REFERRER:
        referrer = "Referrer is empty";
        break;
      case EXPECT_FULL_REFERRER:
        referrer = "Referrer is " + url.spec();
        break;
      case EXPECT_ORIGIN_AS_REFERRER:
        referrer = "Referrer is " + url.GetWithEmptyPath().spec();
        break;
    }
    return base::ASCIIToUTF16(referrer);
  }

  // Adds all possible titles to the TitleWatcher, so we don't time out
  // waiting for the title if the test fails.
  void AddAllPossibleTitles(const GURL& url,
                            content::TitleWatcher* title_watcher) {
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_EMPTY_REFERRER));
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_FULL_REFERRER));
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_ORIGIN_AS_REFERRER));
  }

  enum StartOnProtocol { START_ON_HTTP, START_ON_HTTPS, };

  enum LinkType { REGULAR_LINK, LINK_WITH_TARGET_BLANK, };

  enum RedirectType {
    NO_REDIRECT,        // direct navigation via HTTP
    HTTPS_NO_REDIRECT,  // direct navigation via HTTPS
    SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
    SERVER_REDIRECT_FROM_HTTP_TO_HTTP,
    SERVER_REDIRECT_FROM_HTTP_TO_HTTPS
  };

  enum RendererOrBrowserInitiated { RENDERER_INITIATED, BROWSER_INITIATED };

  // Navigates from a page with a given |referrer_policy| and checks that the
  // reported referrer matches the expectation.
  // Parameters:
  //  referrer_policy:   The referrer policy to test.
  //  start_protocol:    The protocol the test should start on.
  //  link_type:         The link type that is used to trigger the navigation.
  //  redirect:          Whether the link target should redirect and how.
  //  disposition:       The disposition for the navigation.
  //  button:            If not WebMouseEvent::Button::NoButton, click on the
  //                     link with the specified mouse button.
  //  expected_referrer: The kind of referrer to expect.
  //  expected_referrer_policy: The expected referrer policy of the activity.
  //  renderer_or_browser_initiated: If BROWSER_INITIATED, uses Navigate() to
  //  load in the current WebContents and disregards the value of |button|.
  //
  // Returns:
  //  The URL of the first page navigated to.
  GURL RunReferrerTest(const network::mojom::ReferrerPolicy referrer_policy,
                       StartOnProtocol start_protocol,
                       LinkType link_type,
                       RedirectType redirect,
                       WindowOpenDisposition disposition,
                       blink::WebMouseEvent::Button button,
                       ExpectedReferrer expected_referrer,
                       network::mojom::ReferrerPolicy expected_referrer_policy,
                       RendererOrBrowserInitiated
                           renderer_or_browser_initiated = RENDERER_INITIATED) {
    GURL redirect_url;
    switch (redirect) {
      case NO_REDIRECT:
        redirect_url = embedded_test_server()->GetURL(
            "/referrer_policy/referrer-policy-log.html");
        break;
      case HTTPS_NO_REDIRECT:
        redirect_url =
            https_server_.GetURL("/referrer_policy/referrer-policy-log.html");
        break;
      case SERVER_REDIRECT_FROM_HTTPS_TO_HTTP:
        redirect_url = https_server_.GetURL(
            std::string("/server-redirect?") +
            embedded_test_server()
                ->GetURL("/referrer_policy/referrer-policy-log.html")
                .spec());
        break;
      case SERVER_REDIRECT_FROM_HTTP_TO_HTTP:
        redirect_url = embedded_test_server()->GetURL(
            std::string("/server-redirect?") +
            embedded_test_server()
                ->GetURL("/referrer_policy/referrer-policy-log.html")
                .spec());
        break;
      case SERVER_REDIRECT_FROM_HTTP_TO_HTTPS:
        redirect_url = embedded_test_server()->GetURL(
            std::string("/server-redirect?") +
            https_server_.GetURL("/referrer_policy/referrer-policy-log.html")
                .spec());
        break;
    }

    std::string relative_url =
        std::string("/referrer_policy/referrer-policy-start.html?") +
        "policy=" + content::ReferrerPolicyToString(referrer_policy) +
        "&redirect=" + redirect_url.spec() + "&link=" +
        ((button == blink::WebMouseEvent::Button::kNoButton &&
          renderer_or_browser_initiated == RENDERER_INITIATED)
             ? "false"
             : "true") +
        "&target=" + (link_type == LINK_WITH_TARGET_BLANK ? "_blank" : "");

    auto* start_test_server = start_protocol == START_ON_HTTPS
                                  ? &https_server_
                                  : embedded_test_server();
    const GURL start_url = start_test_server->GetURL(relative_url);

    ui_test_utils::AllBrowserTabAddedWaiter add_tab;

    std::u16string expected_title =
        GetExpectedTitle(start_url, expected_referrer);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(tab, expected_title);

    std::string expected_referrer_value;
    if (expected_referrer != EXPECT_EMPTY_REFERRER) {
      expected_referrer_value =
          base::UTF16ToASCII(expected_title)
              .substr(std::string_view("Referrer is ").size());
    }
    base::ReleasableAutoLock releaseable_lock(&check_on_requests_lock_);
    check_on_requests_ = RequestCheck{
        expected_referrer_value, "/referrer_policy/referrer-policy-log.html"};
    releaseable_lock.Release();

    // Watch for all possible outcomes to avoid timeouts if something breaks.
    AddAllPossibleTitles(start_url, &title_watcher);

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

    if (renderer_or_browser_initiated == BROWSER_INITIATED) {
      CHECK(disposition == WindowOpenDisposition::CURRENT_TAB);
      NavigateParams params(browser(), redirect_url, ui::PAGE_TRANSITION_LINK);
      params.referrer = content::Referrer(
          tab->GetController().GetVisibleEntry()->GetURL(), referrer_policy);
      params.source_contents = tab;
      ui_test_utils::NavigateToURL(&params);
    } else if (button != blink::WebMouseEvent::Button::kNoButton) {
      blink::WebMouseEvent mouse_event(
          blink::WebInputEvent::Type::kMouseDown,
          blink::WebInputEvent::kNoModifiers,
          blink::WebInputEvent::GetStaticTimeStampForTests());
      mouse_event.button = button;
      mouse_event.SetPositionInWidget(15, 15);
      mouse_event.click_count = 1;
      tab->GetPrimaryMainFrame()
          ->GetRenderViewHost()
          ->GetWidget()
          ->ForwardMouseEvent(mouse_event);
      mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
      tab->GetPrimaryMainFrame()
          ->GetRenderViewHost()
          ->GetWidget()
          ->ForwardMouseEvent(mouse_event);
    }

    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    } else {
      tab = add_tab.Wait();
      EXPECT_TRUE(tab);
      content::TitleWatcher title_watcher2(tab, expected_title);

      // Watch for all possible outcomes to avoid timeouts if something breaks.
      AddAllPossibleTitles(start_url, &title_watcher2);

      EXPECT_EQ(expected_title, title_watcher2.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_referrer_policy,
              tab->GetController().GetVisibleEntry()->GetReferrer().policy);

    base::AutoLock lock(check_on_requests_lock_);
    check_on_requests_.reset();

    return start_url;
  }

  // Shorthand for cases where |referrer_policy| is the expected policy.
  GURL RunReferrerTest(const network::mojom::ReferrerPolicy referrer_policy,
                       StartOnProtocol start_protocol,
                       LinkType link_type,
                       RedirectType redirect,
                       WindowOpenDisposition disposition,
                       blink::WebMouseEvent::Button button,
                       ExpectedReferrer expected_referrer) {
    return RunReferrerTest(referrer_policy, start_protocol, link_type, redirect,
                           disposition, button, expected_referrer,
                           referrer_policy);
  }

  net::EmbeddedTestServer https_server_;

  // If "check_on_requests_" is set, for each HTTP request that arrives at
  // either of the embedded test servers ("embedded_test_server()" and
  // "https_server_"), if the relative URL equals that stored in
  // "destination_url_to_match", OnServerIncomingRequest will assert
  // that the provided Referer header's value equals the value of
  // "expected_spec".
  struct RequestCheck {
    std::string expected_spec;
    std::string destination_url_to_match;
  };

  base::Lock check_on_requests_lock_;
  std::optional<RequestCheck> check_on_requests_
      GUARDED_BY(check_on_requests_lock_);
};

// The basic behavior of referrer policies is covered by layout tests in
// http/tests/security/referrer-policy-*. These tests cover (hopefully) all
// code paths chrome uses to navigate. To keep the number of combinations down,
// we only test the "origin" policy here.

// Content initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Origin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  REGULAR_LINK, NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsDefault) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  REGULAR_LINK, NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  REGULAR_LINK, NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  REGULAR_LINK, NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickOrigin) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP, REGULAR_LINK,
      NO_REDIRECT, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      blink::WebMouseEvent::Button::kMiddle, EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickOrigin) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      NO_REDIRECT, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      blink::WebMouseEvent::Button::kMiddle, EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  LINK_WITH_TARGET_BLANK, NO_REDIRECT,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  LINK_WITH_TARGET_BLANK, NO_REDIRECT,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  LINK_WITH_TARGET_BLANK, NO_REDIRECT,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickTargetBlankOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  LINK_WITH_TARGET_BLANK, NO_REDIRECT,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP.
// TODO(crbug.com/40804570): Flaky on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ContextMenuOrigin DISABLED_ContextMenuOrigin
#else
#define MAYBE_ContextMenuOrigin ContextMenuOrigin
#endif
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP, REGULAR_LINK,
      NO_REDIRECT, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::WebMouseEvent::Button::kRight, EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP.
// TODO(crbug.com/40803947): Fix flakiness on Linux and Lacros then reenable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HttpsContextMenuOrigin DISABLED_HttpsContextMenuOrigin
#else
#define MAYBE_HttpsContextMenuOrigin HttpsContextMenuOrigin
#endif
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      NO_REDIRECT, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::WebMouseEvent::Button::kRight, EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Redirect) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTPS_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kNoButton, EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsRedirect) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTPS_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kNoButton, EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickRedirect) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTP_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kLeft, EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickRedirect) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTPS_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kLeft, EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_BACKGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_BACKGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  LINK_WITH_TARGET_BLANK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  LINK_WITH_TARGET_BLANK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP via
// server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  LINK_WITH_TARGET_BLANK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP
// via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpsMiddleClickTargetBlankRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  LINK_WITH_TARGET_BLANK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP via server redirect.
// TODO(crbug.com/40803947): Fix flakiness on Linux and Lacros then reenable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ContextMenuRedirect DISABLED_ContextMenuRedirect
#else
#define MAYBE_ContextMenuRedirect ContextMenuRedirect
#endif
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP via server redirect.
// TODO(crbug.com/40804570): Flaky on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HttpsContextMenuRedirect DISABLED_HttpsContextMenuRedirect
#else
#define MAYBE_HttpsContextMenuRedirect HttpsContextMenuRedirect
#endif
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Tests history navigation actions: Navigate from A to B with a referrer
// policy, then navigate to C, back to B, and reload.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, History) {
  // Navigate from A to B.
  GURL start_url = RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTPS_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kLeft, EXPECT_ORIGIN_AS_REFERRER);

  // Navigate to C.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  std::u16string expected_title =
      GetExpectedTitle(start_url, EXPECT_ORIGIN_AS_REFERRER);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::TitleWatcher> title_watcher(
      new content::TitleWatcher(tab, expected_title));

  // Watch for all possible outcomes to avoid timeouts if something breaks.
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Go back to B.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  title_watcher = std::make_unique<content::TitleWatcher>(tab, expected_title);
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Reload to B.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  title_watcher = std::make_unique<content::TitleWatcher>(tab, expected_title);
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Shift-reload to B.
  chrome::ReloadBypassingCache(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
}

// Tests that reloading a site for "request tablet version" correctly clears
// the referrer.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, RequestTabletSite) {
  GURL start_url = RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTPS, REGULAR_LINK,
      SERVER_REDIRECT_FROM_HTTP_TO_HTTP, WindowOpenDisposition::CURRENT_TAB,
      blink::WebMouseEvent::Button::kLeft, EXPECT_ORIGIN_AS_REFERRER);

  std::u16string expected_title =
      GetExpectedTitle(start_url, EXPECT_EMPTY_REFERRER);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, expected_title);

  // Watch for all possible outcomes to avoid timeouts if something breaks.
  AddAllPossibleTitles(start_url, &title_watcher);

  // Erase the current title in the NavigationEntry.
  //
  // TitleWatcher overrides WebContentObserver's TitleWasSet() but also
  // DidStopLoading(). The page that is being reloaded sets its title after load
  // is complete, so the title change is missed because the title is checked on
  // load. Clearing the title ensures that TitleWatcher will wait for the actual
  // title setting.
  tab->GetController().GetVisibleEntry()->SetTitle(std::u16string());

  // Request tablet version.
  chrome::ToggleRequestTabletSite(browser());
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test that an iframes gets the parent frames referrer and referrer policy if
// the load was triggered by the parent, or from the iframe itself, if the
// navigations was started by the iframe.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, IFrame) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kWebKitAllowRunningInsecureContent, true);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::u16string expected_title(u"loaded");
  std::unique_ptr<content::TitleWatcher> title_watcher(
      new content::TitleWatcher(tab, expected_title));

  // Load a page that loads an iframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/referrer_policy/referrer-policy-iframe.html")));
  EXPECT_TRUE(content::ExecJs(
      tab,
      std::string("var frame = document.createElement('iframe');frame.src ='") +
          embedded_test_server()
              ->GetURL("/referrer_policy/referrer-policy-log.html")
              .spec() +
          "';frame.onload = function() { document.title = 'loaded'; };" +
          "document.body.appendChild(frame)"));
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // Verify that the referrer policy was honored and the main page's origin was
  // send as referrer.
  content::RenderFrameHost* frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));
  std::string title = content::EvalJs(frame, "document.title").ExtractString();
  EXPECT_EQ("Referrer is " + https_server_.GetURL("/").spec(), title);

  // Reload the iframe.
  expected_title = u"reset";
  title_watcher = std::make_unique<content::TitleWatcher>(tab, expected_title);
  EXPECT_TRUE(content::ExecJs(tab, "document.title = 'reset'"));
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
  frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));

  expected_title = u"loaded";
  title_watcher = std::make_unique<content::TitleWatcher>(tab, expected_title);
  EXPECT_TRUE(content::ExecJs(frame, "location.reload()"));
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
  frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));

  // Verify that the full url of the iframe was used as referrer.
  title = content::EvalJs(frame, "document.title").ExtractString();
  EXPECT_EQ(
      "Referrer is " + embedded_test_server()
                           ->GetURL("/referrer_policy/referrer-policy-log.html")
                           .spec(),
      title);
}

// Origin When Cross-Origin

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickHTTPSRedirectToHTTPOriginWhenCrossOrigin) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, START_ON_HTTPS,
      REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
      WindowOpenDisposition::CURRENT_TAB, blink::WebMouseEvent::Button::kLeft,
      EXPECT_ORIGIN_AS_REFERRER);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickRedirectToHTTPSOriginWhenCrossOrigin) {
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, START_ON_HTTP,
      REGULAR_LINK, SERVER_REDIRECT_FROM_HTTP_TO_HTTPS,
      WindowOpenDisposition::CURRENT_TAB, blink::WebMouseEvent::Button::kLeft,
      EXPECT_ORIGIN_AS_REFERRER);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickRedirectToHTTPOriginWhenCrossOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
                  START_ON_HTTP, REGULAR_LINK,
                  SERVER_REDIRECT_FROM_HTTP_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_FULL_REFERRER);
}

// Same origin

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickHTTPRedirectToHTTPSameOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kSameOrigin, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTP_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_FULL_REFERRER);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickHTTPRedirectToHTTPSSameOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kSameOrigin, START_ON_HTTPS,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_EMPTY_REFERRER);
}

// Strict origin

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickHTTPRedirectToHTTPStrictOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kStrictOrigin, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTP_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpLeftClickHTTPSRedirectToHTTPStrictOrigin) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kStrictOrigin, START_ON_HTTPS,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_EMPTY_REFERRER);
}

// Parameters for testing functionality imposing ad-hoc restrictions
// on the behavior of referrers, for instance absolute caps like
// "never send referrers" (as of writing, features::kNoReferrers)
// or "on cross-origin requests, never send more than the initiator's
// origin" (features::kCapReferrerToOriginOnCrossOrigin).
//
// These tests assume a default policy of no-referrer-when-downgrade.
struct ReferrerOverrideParams {
  std::optional<base::test::FeatureRef> feature_to_enable;
  network::mojom::ReferrerPolicy baseline_policy;
  network::mojom::ReferrerPolicy expected_policy;

  ReferrerPolicyTest::ExpectedReferrer same_origin_nav,  // HTTP -> HTTP
      cross_origin_nav,                                  // HTTP -> HTTP
      cross_origin_downgrade_nav,  // HTTPS -> HTTP, cross-origin
      same_origin_to_cross_origin_redirect,
      cross_origin_to_same_origin_redirect, same_origin_subresource,
      same_origin_to_cross_origin_subresource_redirect;
} kReferrerOverrideParams[] = {
    {.feature_to_enable = features::kNoReferrers,
     .baseline_policy = network::mojom::ReferrerPolicy::kAlways,
     // The renderer's "have we completely disabled referrers?"
     // implementation resets requests' referrer policies to kNever when
     // it excises their referrers.
     .expected_policy = network::mojom::ReferrerPolicy::kNever,
     .same_origin_nav = ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .cross_origin_nav = ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .cross_origin_downgrade_nav = ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .same_origin_to_cross_origin_redirect =
         ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .cross_origin_to_same_origin_redirect =
         ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .same_origin_subresource = ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
     .same_origin_to_cross_origin_subresource_redirect =
         ReferrerPolicyTest::EXPECT_EMPTY_REFERRER},
    {
        .feature_to_enable = net::features::kCapReferrerToOriginOnCrossOrigin,
        .baseline_policy = network::mojom::ReferrerPolicy::kAlways,
        // Applying the cap doesn't change the "referrer policy"
        // attribute of a request
        .expected_policy = network::mojom::ReferrerPolicy::kAlways,
        .same_origin_nav = ReferrerPolicyTest::EXPECT_FULL_REFERRER,
        .cross_origin_nav = ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .cross_origin_downgrade_nav =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .same_origin_to_cross_origin_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        // Referrer policies get applied to whatever the current referrer is:
        // in the case of a cross-origin -> same-origin redirect, we already
        // will have truncated the referrer to the initiating origin
        .cross_origin_to_same_origin_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .same_origin_subresource = ReferrerPolicyTest::EXPECT_FULL_REFERRER,
        .same_origin_to_cross_origin_subresource_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
    },
    {
        .baseline_policy = network::mojom::ReferrerPolicy::kDefault,
        // kDefault gets resolved into a concrete policy when making requests
        .expected_policy =
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
        .same_origin_nav = ReferrerPolicyTest::EXPECT_FULL_REFERRER,
        .cross_origin_nav = ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .cross_origin_downgrade_nav = ReferrerPolicyTest::EXPECT_EMPTY_REFERRER,
        .same_origin_to_cross_origin_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .cross_origin_to_same_origin_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
        .same_origin_subresource = ReferrerPolicyTest::EXPECT_FULL_REFERRER,
        .same_origin_to_cross_origin_subresource_redirect =
            ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER,
    }};

class ReferrerOverrideTest
    : public ReferrerPolicyTest,
      public ::testing::WithParamInterface<ReferrerOverrideParams> {
 public:
  ReferrerOverrideTest() {
    if (GetParam().feature_to_enable) {
      scoped_feature_list_.InitAndEnableFeature(
          *GetParam().feature_to_enable.value());
    }
  }

 protected:
  // Test that the correct referrer is sent along with
  // a subresource request.
  // Parameter semantics are the same as for
  // ReferrerPolicyTest::RunReferrerTest.
  void RunSubresourceTest(StartOnProtocol start_protocol,
                          RedirectType redirect,
                          network::mojom::ReferrerPolicy baseline_policy,
                          ExpectedReferrer expectation) {
    GURL image_url;
    switch (redirect) {
      case NO_REDIRECT:
        image_url = embedded_test_server()->GetURL("/referrer_policy/logo.gif");
        break;
      case HTTPS_NO_REDIRECT:
        image_url = https_server_.GetURL("/referrer_policy/logo.gif");
        break;
      case SERVER_REDIRECT_FROM_HTTPS_TO_HTTP:
        image_url = https_server_.GetURL(
            std::string("/server-redirect?") +
            embedded_test_server()->GetURL("/referrer_policy/logo.gif").spec());
        break;
      case SERVER_REDIRECT_FROM_HTTP_TO_HTTP:
        image_url = embedded_test_server()->GetURL(
            std::string("/server-redirect?") +
            embedded_test_server()->GetURL("/referrer_policy/logo.gif").spec());
        break;
      case SERVER_REDIRECT_FROM_HTTP_TO_HTTPS:
        image_url = embedded_test_server()->GetURL(
            std::string("/server-redirect?") +
            https_server_.GetURL("/referrer_policy/logo.gif").spec());
        break;
    }

    std::string relative_url =
        std::string("/referrer_policy/referrer-policy-subresource.html?") +
        "policy=" + content::ReferrerPolicyToString(baseline_policy) +
        "&redirect=" + image_url.spec();

    auto* start_server = start_protocol == START_ON_HTTPS
                             ? &https_server_
                             : embedded_test_server();
    const GURL start_url = start_server->GetURL(relative_url);

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::ReleasableAutoLock lock(&check_on_requests_lock_);
    check_on_requests_ = RequestCheck{"", "/referrer_policy/logo.gif"};
    switch (expectation) {
      case ReferrerPolicyTest::EXPECT_EMPTY_REFERRER:
        check_on_requests_->expected_spec = "";
        break;
      case ReferrerPolicyTest::EXPECT_FULL_REFERRER:
        check_on_requests_->expected_spec = start_url.spec();
        break;
      case ReferrerPolicyTest::EXPECT_ORIGIN_AS_REFERRER:
        check_on_requests_->expected_spec = start_url.GetWithEmptyPath().spec();
        break;
    }
    lock.Release();

    // set by referrer-policy-subresource.html JS after the embedded image loads
    std::u16string expected_title(u"loaded");
    std::unique_ptr<content::TitleWatcher> title_watcher(
        new content::TitleWatcher(tab, expected_title));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

    // Wait for the page to load; during the load, since check_on_requests_ is
    // nonempty, OnServerIncomingRequest will validate the referrers.
    EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    WithOverrideParams,
    ReferrerOverrideTest,
    ::testing::ValuesIn(kReferrerOverrideParams),
    [](const ::testing::TestParamInfo<ReferrerOverrideParams>& info)
        -> std::string {
      if (info.param.feature_to_enable) {
        return base::StringPrintf("Param%s",
                                  info.param.feature_to_enable.value()->name);
      }
      return "NoFeature";
    });

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, SameOriginNavigation) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTP, REGULAR_LINK,
                  NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  GetParam().same_origin_nav, GetParam().expected_policy);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, CrossOriginNavigation) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTP, REGULAR_LINK,
                  HTTPS_NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  GetParam().cross_origin_nav, GetParam().expected_policy);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest,
                       CrossOriginNavigationBrowserInitiated) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTP, REGULAR_LINK,
                  HTTPS_NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  GetParam().cross_origin_nav, GetParam().expected_policy,
                  BROWSER_INITIATED);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, CrossOriginDowngradeNavigation) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTPS, REGULAR_LINK,
                  NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  GetParam().cross_origin_downgrade_nav,
                  GetParam().expected_policy);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, CrossOriginRedirect) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTP, REGULAR_LINK,
                  SERVER_REDIRECT_FROM_HTTP_TO_HTTPS,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  GetParam().same_origin_to_cross_origin_redirect,
                  GetParam().expected_policy);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, CrossOriginToSameOriginRedirect) {
  RunReferrerTest(GetParam().baseline_policy, START_ON_HTTP, REGULAR_LINK,
                  SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kNoButton,
                  GetParam().cross_origin_to_same_origin_redirect,
                  GetParam().expected_policy);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest, SameOriginSubresource) {
  RunSubresourceTest(START_ON_HTTP, NO_REDIRECT, GetParam().baseline_policy,
                     GetParam().same_origin_subresource);
}

IN_PROC_BROWSER_TEST_P(ReferrerOverrideTest,
                       SameOriginToCrossOriginSubresourceRedirect) {
  RunSubresourceTest(
      START_ON_HTTP, SERVER_REDIRECT_FROM_HTTP_TO_HTTPS,
      GetParam().baseline_policy,
      GetParam().same_origin_to_cross_origin_subresource_redirect);
}

// Most of the functionality of the referrer-cap flag is covered by
// ReferrerOverrideTest; these couple additional tests test the flag's
// interaction with other referrer policies
class ReferrerPolicyCapReferrerToOriginOnCrossOriginTest
    : public ReferrerPolicyTest {
 public:
  ReferrerPolicyCapReferrerToOriginOnCrossOriginTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kCapReferrerToOriginOnCrossOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that capping referrer granularity at origin on cross-origin requests
// correctly defers to a more restrictive referrer policy on a
// cross-origin navigation.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyCapReferrerToOriginOnCrossOriginTest,
                       HonorsMoreRestrictivePolicyOnNavigation) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kSameOrigin, START_ON_HTTPS,
                  REGULAR_LINK, NO_REDIRECT /*direct navigation x-origin*/,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_EMPTY_REFERRER);
}

// Test that capping referrer granularity at origin on cross-origin requests
// correctly defers to a more restrictive referrer policy on a
// cross-origin redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyCapReferrerToOriginOnCrossOriginTest,
                       HonorsMoreRestrictivePolicyOnRedirect) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kStrictOrigin, START_ON_HTTPS,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_EMPTY_REFERRER);
}

// Test that, when the cross-origin referrer cap is on but we also have the
// "no referrers at all" pref set, we send no referrer at all on cross-origin
// requests.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyCapReferrerToOriginOnCrossOriginTest,
                       RespectsNoReferrerPref) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kEnableReferrers, false);
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  RunReferrerTest(network::mojom::ReferrerPolicy::kAlways, START_ON_HTTPS,
                  REGULAR_LINK, NO_REDIRECT, WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_EMPTY_REFERRER,
                  // when the pref is set, the renderer sets the referrer policy
                  // to the kNever on outgoing requests at the same time
                  // it removes referrers
                  network::mojom::ReferrerPolicy::kNever);
}
