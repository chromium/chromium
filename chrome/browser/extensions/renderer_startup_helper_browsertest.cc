// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

// This test exercises support for kInitialWebUIWithoutExtensions, a performance
// optimization for WebUI "top chrome" which is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {

namespace {

// A generic factory that returns a base WebUIController for any WebUI URL.
// This avoids the dependency-heavy WebUIToolbarUI and its associated crashes.
class WebUITestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    return content::HasWebUIScheme(url)
               ? std::make_unique<content::WebUIController>(web_ui)
               : nullptr;
  }
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    return content::HasWebUIScheme(url)
               ? reinterpret_cast<content::WebUI::TypeID>(1)
               : nullptr;
  }
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return content::HasWebUIScheme(url);
  }
};

// Overrides the BrowserClient to mark our dummy URL as an "Initial WebUI".
class InitialWebUIOverrideChromeContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  explicit InitialWebUIOverrideChromeContentBrowserClient(
      const GURL& initial_webui_url)
      : initial_webui_url_(initial_webui_url) {}

  bool IsInitialWebUIURL(const GURL& url) override {
    return initial_webui_url_ == url;
  }

  bool IsTopChromeWebUIURL(const GURL& url) override {
    return initial_webui_url_ == url;
  }

 private:
  GURL initial_webui_url_;
};

}  // namespace

class RendererStartupHelperBrowserTest : public InProcessBrowserTest {
 public:
  RendererStartupHelperBrowserTest() {
    content::WebUIControllerFactory::RegisterFactory(
        &webui_controller_factory_);

    // Enable the feature and any WebUI dependencies.
    feature_list_.InitWithFeatures(
        {blink::features::kInitialWebUIWithoutExtensions,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

 protected:
  // Helper to check if a process is initialized for extensions.
  bool IsInitialized(content::RenderProcessHost* process) {
    return RendererStartupHelperFactory::GetForBrowserContext(
               process->GetBrowserContext())
        ->IsProcessInitializedForTesting(process);
  }

  content::WebContents* contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  WebUITestWebUIControllerFactory webui_controller_factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RendererStartupHelperBrowserTest,
                       DeferredInitializationPerNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url("chrome://test-initial/");

  // Mark our test URL as an "Initial WebUI" URL.
  InitialWebUIOverrideChromeContentBrowserClient test_client(initial_url);
  content::ContentBrowserClient* old_client =
      content::SetBrowserClientForTesting(&test_client);

  // Create a new WebContents manually to ensure it is the "first navigation."
  content::WebContents::CreateParams params(contents()->GetBrowserContext());
  params.site_instance = content::SiteInstance::CreateForURL(
      contents()->GetBrowserContext(), initial_url);
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(params);

  // Force process initialization. This should trigger OnRenderProcessCreated.
  // Because we marked it as Initial WebUI, initialization should be skipped.
  EXPECT_TRUE(new_web_contents->GetPrimaryMainFrame()->GetProcess()->Init());

  content::RenderProcessHost* process =
      new_web_contents->GetPrimaryMainFrame()->GetProcess();

  // Verify the process should NOT be initialized for extensions.
  EXPECT_FALSE(IsInitialized(process));

  content::SetBrowserClientForTesting(old_client);

  // Navigate to a regular site in the same WebContents.
  GURL normal_url = embedded_test_server()->GetURL("/simple.html");
  content::TestNavigationObserver observer2(new_web_contents.get());
  new_web_contents->GetController().LoadURL(normal_url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            std::string());
  observer2.Wait();

  // Verify the process should now be initialized.
  EXPECT_TRUE(
      IsInitialized(new_web_contents->GetPrimaryMainFrame()->GetProcess()));
}

}  // namespace extensions
