// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "extensions/browser/api/extensions_api_client.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
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

  void VerifyWebviewURLForState(const std::string& expected_state_value) {
    auto* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    views::Widget* widget =
        service->opt_in_controller().ShowDialog(web_contents);
    ASSERT_TRUE(widget);
    views::test::WidgetVisibleWaiter(widget).Wait();
    EXPECT_TRUE(widget->IsVisible());

    auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
    ASSERT_TRUE(guest_view);
    content::WebContents* guest_contents = guest_view->web_contents();
    ASSERT_TRUE(guest_contents);

    EXPECT_TRUE(content::WaitForLoadStop(guest_contents));
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

    service->opt_in_controller().CloseDialog();
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::Widget* widget =
      service()->opt_in_controller().ShowDialog(web_contents);
  ASSERT_TRUE(widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  views::View* view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          GlicExperimentalOptInDialogView::kDialogElementId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_TRUE(view);
  views::WebView* web_view = static_cast<views::WebView*>(view);
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

  views::Widget* widget = service()->opt_in_controller().ShowDialog(tab1);
  ASSERT_TRUE(widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

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
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  // Set FRE to incomplete to ensure HasConsented is false.
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service->enabling().HasConsented());

  VerifyWebviewURLForState("glic");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ActuationOptInState) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  // Set Glic FRE completed, but actuation disabled.
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service->enabling().SetUserEnabledActuationOnWeb(false);

  VerifyWebviewURLForState("actuation");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ExperimentalOptInState) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  // Set Glic FRE completed, actuation enabled, but experimental triggering
  // disabled.
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service->enabling().SetUserEnabledActuationOnWeb(true);
  service->enabling().SetExperimentalTriggeringEnabled(false);

  VerifyWebviewURLForState("experimental");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, WebviewURL_OptInNotNeeded) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  // Set Glic FRE completed, actuation enabled, AND experimental triggering
  // enabled.
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service->enabling().SetUserEnabledActuationOnWeb(true);
  service->enabling().SetExperimentalTriggeringEnabled(true);

  // Verify ShowDialog returns nullptr since opt-in is already complete!
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::Widget* widget = service->opt_in_controller().ShowDialog(web_contents);
  EXPECT_FALSE(widget);
}

}  // namespace glic
