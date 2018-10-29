// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/platform/web_input_event.h"

class ReferrerPolicyTest : public InProcessBrowserTest {
 public:
  ReferrerPolicyTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(https_server_.Start());
  }
  ~ReferrerPolicyTest() override {}

 protected:
  enum ExpectedReferrer {
    EXPECT_EMPTY_REFERRER,
    EXPECT_FULL_REFERRER,
    EXPECT_ORIGIN_AS_REFERRER
  };

  // Returns the expected title for the tab with the given (full) referrer and
  // the expected modification of it.
  base::string16 GetExpectedTitle(const GURL& url,
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

  // Returns a string representation of a given |referrer_policy|.
  std::string ReferrerPolicyToString(
      network::mojom::ReferrerPolicy referrer_policy) {
    switch (referrer_policy) {
      case network::mojom::ReferrerPolicy::kDefault:
        return "no-meta";
      case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
        return "default";
      case network::mojom::ReferrerPolicy::kOrigin:
        return "origin";
      case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
        return "origin-when-crossorigin";
      case network::mojom::ReferrerPolicy::kSameOrigin:
        return "same-origin";
      case network::mojom::ReferrerPolicy::kStrictOrigin:
        return "strict-origin";
      case network::mojom::ReferrerPolicy::kAlways:
        return "always";
      case network::mojom::ReferrerPolicy::kNever:
        return "never";
      case network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
        return "reduce-referrer-granularity";
    }
    NOTREACHED();
    return "";
  }

  enum StartOnProtocol { START_ON_HTTP, START_ON_HTTPS, };

  enum LinkType { REGULAR_LINK, LINK_WITH_TARGET_BLANK, };

  enum RedirectType {
    NO_REDIRECT,
    SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
    SERVER_REDIRECT_FROM_HTTP_TO_HTTP,
    SERVER_REDIRECT_FROM_HTTP_TO_HTTPS
  };

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
  //
  // Returns:
  //  The URL of the first page navigated to.
  GURL RunReferrerTest(
      const network::mojom::ReferrerPolicy referrer_policy,
      StartOnProtocol start_protocol,
      LinkType link_type,
      RedirectType redirect,
      WindowOpenDisposition disposition,
      blink::WebMouseEvent::Button button,
      ExpectedReferrer expected_referrer,
      network::mojom::ReferrerPolicy expected_referrer_policy) {
    GURL redirect_url;
    switch (redirect) {
      case NO_REDIRECT:
        redirect_url = embedded_test_server()->GetURL(
            "/referrer_policy/referrer-policy-log.html");
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
        "policy=" + ReferrerPolicyToString(referrer_policy) +
        "&redirect=" + redirect_url.spec() + "&link=" +
        (button == blink::WebMouseEvent::Button::kNoButton ? "false" : "true") +
        "&target=" + (link_type == LINK_WITH_TARGET_BLANK ? "_blank" : "");

    auto* start_test_server = start_protocol == START_ON_HTTPS
                                  ? &https_server_
                                  : embedded_test_server();
    const GURL start_url = start_test_server->GetURL(relative_url);

    ui_test_utils::WindowedTabAddedNotificationObserver tab_added_observer(
        content::NotificationService::AllSources());

    base::string16 expected_title =
        GetExpectedTitle(start_url, expected_referrer);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(tab, expected_title);

    // Watch for all possible outcomes to avoid timeouts if something breaks.
    AddAllPossibleTitles(start_url, &title_watcher);

    ui_test_utils::NavigateToURL(browser(), start_url);

    if (button != blink::WebMouseEvent::Button::kNoButton) {
      blink::WebMouseEvent mouse_event(
          blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
          blink::WebInputEvent::GetStaticTimeStampForTests());
      mouse_event.button = button;
      mouse_event.SetPositionInWidget(15, 15);
      mouse_event.click_count = 1;
      tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
      mouse_event.SetType(blink::WebInputEvent::kMouseUp);
      tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
    }

    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    } else {
      tab_added_observer.Wait();
      tab = tab_added_observer.GetTab();
      EXPECT_TRUE(tab);
      content::TitleWatcher title_watcher2(tab, expected_title);

      // Watch for all possible outcomes to avoid timeouts if something breaks.
      AddAllPossibleTitles(start_url, &title_watcher2);

      EXPECT_EQ(expected_title, title_watcher2.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_referrer_policy,
              tab->GetController().GetVisibleEntry()->GetReferrer().policy);

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
};

class ReferrerPolicyWithReduceReferrerGranularityFlagTest
    : public ReferrerPolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kReducedReferrerGranularity);
  }
};

// The basic behavior of referrer policies is covered by layout tests in
// http/tests/security/referrer-policy-*. These tests cover (hopefully) all
// code paths chrome uses to navigate. To keep the number of combinations down,
// we only test the "origin" policy here.
//
// Some tests are marked as FAILS, see http://crbug.com/124750

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
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, ContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(
      network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP, REGULAR_LINK,
      NO_REDIRECT, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::WebMouseEvent::Button::kRight, EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsContextMenuOrigin) {
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
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, ContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(network::mojom::ReferrerPolicy::kOrigin, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTPS_TO_HTTP,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  blink::WebMouseEvent::Button::kRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsContextMenuRedirect) {
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
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  base::string16 expected_title =
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

  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Reload to B.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
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

  base::string16 expected_title =
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
  tab->GetController().GetVisibleEntry()->SetTitle(base::string16());

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
  base::string16 expected_title(base::ASCIIToUTF16("loaded"));
  std::unique_ptr<content::TitleWatcher> title_watcher(
      new content::TitleWatcher(tab, expected_title));

  // Load a page that loads an iframe.
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/referrer_policy/referrer-policy-iframe.html"));
  EXPECT_TRUE(content::ExecuteScript(
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
      tab, base::Bind(&content::FrameIsChildOfMainFrame));
  std::string title;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      frame,
      "window.domAutomationController.send(document.title)",
      &title));
  EXPECT_EQ("Referrer is " + https_server_.GetURL("/").spec(), title);

  // Reload the iframe.
  expected_title = base::ASCIIToUTF16("reset");
  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
  EXPECT_TRUE(content::ExecuteScript(tab, "document.title = 'reset'"));
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  expected_title = base::ASCIIToUTF16("loaded");
  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
  EXPECT_TRUE(content::ExecuteScript(frame, "location.reload()"));
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // Verify that the full url of the iframe was used as referrer.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      frame,
      "window.domAutomationController.send(document.title)",
      &title));
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

// Reduced 'referer' granularity flag tests.

// User initiated navigation, from HTTP to HTTPS via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpLeftClickRedirectDefaultNoFlag) {
  RunReferrerTest(network::mojom::ReferrerPolicy::kDefault, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTP_TO_HTTPS,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft, EXPECT_FULL_REFERRER,
                  network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyWithReduceReferrerGranularityFlagTest,
                       HttpLeftClickRedirectDefaultFlag) {
  network::mojom::ReferrerPolicy expected_referrer_policy = network::mojom::
      ReferrerPolicy::kNoReferrerWhenDowngradeOriginWhenCrossOrigin;

  RunReferrerTest(network::mojom::ReferrerPolicy::kDefault, START_ON_HTTP,
                  REGULAR_LINK, SERVER_REDIRECT_FROM_HTTP_TO_HTTPS,
                  WindowOpenDisposition::CURRENT_TAB,
                  blink::WebMouseEvent::Button::kLeft,
                  EXPECT_ORIGIN_AS_REFERRER, expected_referrer_policy);
}
