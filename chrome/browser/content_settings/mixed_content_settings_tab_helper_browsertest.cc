// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class MixedContentSettingsTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  MixedContentSettingsTabHelperBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &MixedContentSettingsTabHelperBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~MixedContentSettingsTabHelperBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(&ssl_server_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ssl_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(ssl_server_.Start());
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  net::EmbeddedTestServer* test_server() { return &ssl_server_; }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  net::EmbeddedTestServer ssl_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Tests that openers can share their settings with the WebContents they opened.
IN_PROC_BROWSER_TEST_F(MixedContentSettingsTabHelperBrowserTest,
                       OpenerSharePageSetting) {
  GURL opener_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));
  GURL new_tab_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html?new"));
  auto* opener_helper =
      MixedContentSettingsTabHelper::FromWebContents(web_contents());

  // Loads a primary page that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), opener_url));

  // Mixed content should be blocked at first.
  EXPECT_FALSE(
      opener_helper->IsRunningInsecureContentAllowed(*current_frame_host()));
  content::WebContents* opener_contents = web_contents();

  // Emulates link clicking on the mixed script bubble to allow mixed content
  // to run.
  content::TestNavigationObserver observer(web_contents());
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(), web_contents(),
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Waits for reload.
  observer.Wait();

  // Mixed content should no longer be blocked.
  EXPECT_TRUE(
      opener_helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Open a new tab.
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  content::ExecuteScriptAsync(
      opener_contents, content::JsReplace("window.open($1);", new_tab_url));
  tab_added_waiter.Wait();
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_contents);
  ASSERT_NE(new_contents, opener_contents);

  // Mixed content should not be blocked in the newly opened tab.
  auto* new_contents_helper =
      MixedContentSettingsTabHelper::FromWebContents(new_contents);
  EXPECT_TRUE(new_contents_helper->IsRunningInsecureContentAllowed(
      *new_contents->GetMainFrame()));
}

// Tests that the prerending doesn't affect the mixed content's insecure status
// in the primary page.
IN_PROC_BROWSER_TEST_F(MixedContentSettingsTabHelperBrowserTest,
                       KeepInsecureInPrerendering) {
  GURL primary_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));
  auto* helper = MixedContentSettingsTabHelper::FromWebContents(web_contents());

  // Loads a primary page that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), primary_url));

  // Mixed content should be blocked at first.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Emulates link clicking on the mixed script bubble to allow mixed content
  // to run.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Waits for reload.
  observer.Wait();

  // Mixed content should no longer be blocked.
  EXPECT_TRUE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Loads a page in the prerendering.
  GURL prerender_url(test_server()->GetURL(
      "/content_setting_bubble/mixed_script.html?prerendering"));
  const int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Mixed content should be blocked in the prerendering page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*prerender_rfh));

  // Mixed content should keep to be unblocked in the primary page.
  EXPECT_TRUE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  // Mixed content should be blocked in the new page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));
}

// Tests that the prerending doesn't affect the mixed content's insecure status
// with the main frame.
IN_PROC_BROWSER_TEST_F(MixedContentSettingsTabHelperBrowserTest,
                       DoNotAffectInsecureOfPrimaryPageInPrerendering) {
  GURL primary_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));

  auto* helper = MixedContentSettingsTabHelper::FromWebContents(web_contents());

  // Loads a primary page that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), primary_url));

  // Mixed content should be blocked in the page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Loads a page in the prerendering.
  GURL prerender_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));
  const int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Mixed content should be blocked in the prerendering page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*prerender_rfh));

  helper->AllowRunningOfInsecureContent(*prerender_rfh);

  // Mixed content should be unblocked in the prerendering page.
  EXPECT_TRUE(helper->IsRunningInsecureContentAllowed(*prerender_rfh));

  // Mixed content should keep to be blocked in the primary page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  // Mixed content should keep to be unblocked in the new page.
  EXPECT_TRUE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));
}

// Tests that the activated page keeps the mixed content's secure status
// after the prerending page is activated.
IN_PROC_BROWSER_TEST_F(MixedContentSettingsTabHelperBrowserTest,
                       DoNotAffectSecureOfPrerenderingPage) {
  GURL primary_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));

  auto* helper = MixedContentSettingsTabHelper::FromWebContents(web_contents());

  // Loads a primary page that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), primary_url));

  // Mixed content should be blocked in the activated page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Loads a page in the prerendering.
  GURL prerender_url(
      test_server()->GetURL("/content_setting_bubble/mixed_script.html"));
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Mixed content should be blocked in the prerendering page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*prerender_rfh));

  helper->AllowRunningOfInsecureContent(*current_frame_host());

  // Mixed content should be unblocked in the activated page.
  EXPECT_TRUE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));

  // Mixed content should keep to be blocked in the prerendering page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*prerender_rfh));

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  // Mixed content should keep to be blocked in the activated page.
  EXPECT_FALSE(helper->IsRunningInsecureContentAllowed(*current_frame_host()));
}
