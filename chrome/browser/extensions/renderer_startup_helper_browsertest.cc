// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension_features.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kInitialWebUIHost[] = "test-initial";

// A generic factory that returns a base WebUIController for any WebUI URL.
// This avoids the dependency-heavy WebUIToolbarUI and its associated crashes.
class WebUITestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    return url.host() == kInitialWebUIHost
               ? std::make_unique<content::WebUIController>(web_ui)
               : nullptr;
  }
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    return url.host() == kInitialWebUIHost
               ? reinterpret_cast<content::WebUI::TypeID>(1)
               : nullptr;
  }
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return url.host() == kInitialWebUIHost;
  }
};

// Enables in-process resource loading V2 for the initial WebUI host.
class InitialWebUITestConfig
    : public content::DefaultWebUIConfig<content::WebUIController> {
 public:
  InitialWebUITestConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kInitialWebUIHost) {}
  bool SupportsInProcessResourceLoadingV2() const override { return true; }
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

class RendererStartupHelperBrowserTest : public PlatformBrowserTest {
 public:
  RendererStartupHelperBrowserTest() {
    content::WebUIControllerFactory::RegisterFactory(
        &webui_controller_factory_);

    feature_list_.InitWithFeatures(
        {extensions_features::kInitialWebUIWithoutExtensions,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    // Initial WebUI navigations load the document body in the renderer, which
    // requires in-process resource loading V2.
    content::WebUIConfigMap::GetInstance().AddWebUIConfig(
        std::make_unique<InitialWebUITestConfig>());
    content::WebUIDataSource::CreateAndAdd(browser_context(), kInitialWebUIHost)
        ->SetResourcePathToResponse("", "<!doctype html><body>bar</body>");
  }

  void TearDownOnMainThread() override {
    content::WebUIConfigMap::GetInstance().RemoveConfig(InitialWebUIURL());
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  static GURL InitialWebUIURL() {
    return GURL(base::StrCat({"chrome://", kInitialWebUIHost, "/"}));
  }

  // Helper to check if a process is initialized for extensions.
  bool IsInitialized(content::RenderProcessHost* process) {
    return RendererStartupHelperFactory::GetForBrowserContext(
               process->GetBrowserContext())
        ->IsProcessInitializedForTesting(process);
  }

  content::BrowserContext* browser_context() {
    return chrome_test_utils::GetProfile(this);
  }

 private:
  WebUITestWebUIControllerFactory webui_controller_factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RendererStartupHelperBrowserTest,
                       DeferredInitializationPerNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = InitialWebUIURL();

  // Mark our test URL as an "Initial WebUI" URL.
  InitialWebUIOverrideChromeContentBrowserClient test_client(initial_url);
  content::ContentBrowserClient* old_client =
      content::SetBrowserClientForTesting(&test_client);

  // Create a new WebContents manually to ensure it is the "first navigation."
  content::WebContents::CreateParams params(browser_context());
  params.site_instance =
      content::SiteInstance::CreateForURL(browser_context(), initial_url);
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(params);

  // Force process initialization. This should trigger OnRenderProcessCreated.
  // Because we marked it as Initial WebUI, initialization should be skipped.
  EXPECT_TRUE(new_web_contents->GetPrimaryMainFrame()->GetProcess()->Init());

  content::RenderProcessHost* process =
      new_web_contents->GetPrimaryMainFrame()->GetProcess();

  // Verify the process should NOT be initialized for extensions.
  EXPECT_FALSE(IsInitialized(process));

  // Navigate to the Initial WebUI URL. Initialization should still be skipped
  // when the navigation is ready to commit.
  content::TestNavigationObserver observer1(new_web_contents.get());
  new_web_contents->GetController().LoadURL(initial_url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            std::string());
  observer1.Wait();
  EXPECT_TRUE(observer1.last_navigation_succeeded());
  EXPECT_EQ(process, new_web_contents->GetPrimaryMainFrame()->GetProcess());
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
