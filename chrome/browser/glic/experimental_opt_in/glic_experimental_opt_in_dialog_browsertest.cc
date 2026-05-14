// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace glic {

class GlicExperimentalOptInTest : public GlicBrowserTest {
 public:
  // These tests don't run on Android, so allow browser() use.
  using PlatformBrowserTest::browser;
  GlicExperimentalOptInTest() = default;
  ~GlicExperimentalOptInTest() override = default;

  void SetUp() override {
    opt_in_test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/webui/glic/");
    ASSERT_TRUE(opt_in_test_server_.InitializeAndListen());
    GURL test_url =
        opt_in_test_server_.GetURL("a.test", "/test_data/page.html");

    base::FieldTrialParams params;
    params["glic-experimental-triggering-opt-in-url"] = test_url.spec();
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicExperimentalTriggering, params);

    GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    opt_in_test_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");
    creation_subscription_ = content::RegisterWebContentsCreationCallback(
        base::BindRepeating(&GlicExperimentalOptInTest::OnWebContentsCreated,
                            base::Unretained(this)));
  }

  guest_view::TestGuestViewManager* GetGuestViewManager() {
    return guest_view_manager_factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                  ->CreateGuestViewManagerDelegate());
  }

  views::Widget* ShowDialogAndWait(
      content::WebContents* web_contents = nullptr) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    views::Widget* widget =
        service()->opt_in_controller().ShowDialog(web_contents);
    if (!widget) {
      return nullptr;
    }
    views::test::WidgetVisibleWaiter(widget).Wait();
    EXPECT_TRUE(widget->IsVisible());
    return widget;
  }

  content::WebContents* WaitForGuestContents() {
    auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
    if (!guest_view) {
      return nullptr;
    }
    content::WebContents* guest_contents = guest_view->web_contents();
    if (!guest_contents) {
      return nullptr;
    }
    EXPECT_TRUE(content::WaitForLoadStop(guest_contents));
    return guest_contents;
  }

  void VerifyWebviewURLForState(const std::string& expected_state_value) {
    views::Widget* widget = ShowDialogAndWait();
    ASSERT_TRUE(widget);

    content::WebContents* guest_contents = WaitForGuestContents();
    ASSERT_TRUE(guest_contents);

    ASSERT_TRUE(guest_contents->GetController().GetLastCommittedEntry());
    EXPECT_EQ(
        guest_contents->GetController().GetLastCommittedEntry()->GetPageType(),
        content::PAGE_TYPE_NORMAL);
    GURL actual_url = guest_contents->GetLastCommittedURL();

    GURL expected_url =
        opt_in_test_server_.GetURL("a.test", "/test_data/page.html");
    expected_url = net::AppendOrReplaceQueryParameter(
        expected_url, "experimental_triggering_opt_in", expected_state_value);
    expected_url = DecorateGlicFreUrl(browser()->profile(), expected_url);
    EXPECT_EQ(actual_url, expected_url);

    service()->opt_in_controller().CloseDialog();
  }

 private:
  // In a stripped-down browser test environment, dynamically created guest
  // WebContents inside <webview> lack critical extensions observers. We
  // manually attach them here to enable IPC bindings and navigation success.
  void OnWebContentsCreated(content::WebContents* web_contents) {
    extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
        web_contents);
    extensions::TabHelper::CreateForWebContents(web_contents);
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription creation_subscription_;
  net::EmbeddedTestServer opt_in_test_server_;
  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpensDialog) {
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  content::WebContents* dialog_contents = web_view->GetWebContents();
  ASSERT_TRUE(dialog_contents);

  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));
  ASSERT_TRUE(dialog_contents->GetController().GetLastCommittedEntry());
  EXPECT_EQ(
      dialog_contents->GetController().GetLastCommittedEntry()->GetPageType(),
      content::PAGE_TYPE_NORMAL);
  EXPECT_EQ(dialog_contents->GetLastCommittedURL(),
            GURL(chrome::kChromeUIGlicExperimentalOptInURL));

  service()->opt_in_controller().CloseDialog();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, TabModality) {
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::Widget* widget = ShowDialogAndWait(tab1);
  ASSERT_TRUE(widget);

  // Open a new tab.
  chrome::AddSelectedTabWithURL(browser(), GURL("about:blank"),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab1, tab2);

  // The dialog should be hidden.
  views::test::WidgetVisibleWaiter(widget).WaitUntilInvisible();
  EXPECT_FALSE(widget->IsVisible());

  // Switch back to tab1.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(tab1, browser()->tab_strip_model()->GetActiveWebContents());

  // The dialog should be visible again.
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  // Cleanup.
  service()->opt_in_controller().CloseDialog();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, WebviewURL_GlicOptInState) {
  // Set FRE to incomplete to ensure HasConsented is false.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  VerifyWebviewURLForState("glic");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ActuationOptInState) {
  // Set Glic FRE completed, but actuation disabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(false);

  VerifyWebviewURLForState("actuation");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ExperimentalOptInState) {
  // Set Glic FRE completed, actuation enabled, but experimental triggering
  // disabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(false);

  VerifyWebviewURLForState("experimental");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, WebviewURL_OptInNotNeeded) {
  // Set Glic FRE completed, actuation enabled, AND experimental triggering
  // enabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(true);

  // Verify ShowDialog returns nullptr since opt-in is already complete!
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::Widget* widget =
      service()->opt_in_controller().ShowDialog(web_contents);
  EXPECT_FALSE(widget);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, ResizesToContent) {
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "a.test") {
          std::string html = R"(
            <html>
              <body style="width: 500px; height: 400px; margin: 0;">
                <div style="width: 100%; height: 100%; background: blue;"></div>
              </body>
            </html>
          )";
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-type: text/html\n\n", html,
              params->client.get());
          return true;
        }
        return false;
      }));

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Verify that the WebView's preferred size resizes to match the loaded
  // content.
  bool size_matched = base::test::RunUntil([web_view]() {
    return web_view->GetPreferredSize() == gfx::Size(512, 400);
  });

  EXPECT_TRUE(size_matched) << "Timed out waiting for web_view to resize to "
                               "512x400! Current preferred size: "
                            << web_view->GetPreferredSize().ToString();

  service()->opt_in_controller().CloseDialog();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       TabClosedClosesDialogSynchronously) {
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Track the view and widget lifetime.
  views::ViewTracker tracker(web_view);
  EXPECT_TRUE(tracker.view());
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Close the tab (triggers dialog close).
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  // The view and widget should be destroyed synchronously.
  EXPECT_FALSE(tracker.view());
  destroyed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, TabDraggedToAnotherWindow) {
  // Add a second tab so we may detach the first.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, false);

  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  views::Widget* widget = ShowDialogAndWait(tab1);
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Track the view and widget lifetime.
  views::ViewTracker tracker(web_view);
  EXPECT_TRUE(tracker.view());
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Detach the tab (triggers kInsertIntoOtherWindow detach reason and sync
  // dialog close).
  std::unique_ptr<content::WebContents> detached_contents =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  // The view and widget should be destroyed synchronously due to
  // MakeCloseSynchronous in the controller.
  EXPECT_FALSE(tracker.view());
  destroyed_waiter.Wait();

  // Re-insert the detached WebContents into the tab strip so it is cleanly
  // destroyed during browser teardown.
  browser()->tab_strip_model()->AppendWebContents(std::move(detached_contents),
                                                  true);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInGlic) {
  // Set required state to Glic.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Change location hash to #continue to simulate user accepting the opt-in.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

  // Wait for the widget to close.
  views::test::WidgetDestroyedWaiter(widget).Wait();

  // Verify that Glic is consented, Actuation is enabled, AND Experimental is
  // enabled (3 opt-ins).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, RejectOptIn) {
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Change location hash to #noThanks.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#noThanks';"));

  // Wait for the widget to close.
  views::test::WidgetDestroyedWaiter(widget).Wait();

  // Verify Glic is still not consented.
  EXPECT_FALSE(service()->enabling().HasConsented());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInActuation) {
  // Set required state to Actuation (Glic complete, Actuation incomplete).
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(false);
  ASSERT_TRUE(service()->enabling().HasConsented());

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Accept opt-in.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget).Wait();

  // Verify Glic remains complete, and Actuation AND Experimental are enabled (2
  // opt-ins).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInExperimental) {
  // Set Glic complete, Actuation complete, but Experimental Triggering
  // incomplete.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(false);

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Accept opt-in.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget).Wait();

  // Verify Glic is consented, Actuation remains enabled, and Experimental is
  // enabled (1 opt-in).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());
}

}  // namespace glic
