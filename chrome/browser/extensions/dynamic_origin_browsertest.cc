// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// Test dynamic origins in web accessible resources.
// TODO(crbug.com/352267920): Move to web_accessible_resources_browsertest.cc?
class DynamicOriginBrowserTest : public ExtensionBrowserTest {
 public:
  DynamicOriginBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionDynamicURLRedirection);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    InstallExtension();
  }

 protected:
  const Extension* GetExtension() { return extension_; }

  content::WebContents* GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() const {
    return GetActiveWebContents()->GetPrimaryMainFrame();
  }

 private:
  void InstallExtension() {
    dir_.WriteManifest(R"({
      "name": "Extension",
      "version": "1.0",
      "manifest_version": 3,
      "web_accessible_resources": [{
        "resources": ["web_accessible_resource.html", "ok.html"],
        "matches": ["<all_urls>"]
      }]
    })");
    std::vector<std::string> files(
        {"extension_resource.html", "web_accessible_resource.html", "ok.html"});
    for (const auto& filename : files) {
      dir_.WriteFile(base::FilePath::FromASCII(filename).value(), filename);
    }
    extension_ = LoadExtension(dir_.UnpackedPath());
    DCHECK(extension_);
  }

  raw_ptr<const Extension, DanglingUntriaged> extension_ = nullptr;
  TestExtensionDir dir_;
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
};

// Test a dynamic url as a web accessible resource.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest, DynamicUrl) {
  auto* extension = GetExtension();

  // Resource and extension origin should match.
  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL("ok.html")));
    ASSERT_EQ(extension->origin(),
              GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }

  // Dynamic resource should resolve to static url.
  {
    GURL static_url = extension->url().Resolve("ok.html");
    GURL dynamic_url = extension->dynamic_url().Resolve("ok.html");
    ASSERT_NE(static_url, dynamic_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dynamic_url));
    EXPECT_EQ(static_url, GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(extension->origin(),
              GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }
}

// Error accessing resource from random guid.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest,
                       InvalidDynamicResourceFailsToLoad) {
  auto* extension = GetExtension();

  auto run = [&](const GURL& gurl, int status) {
    content::WebContents* web_contents = GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents);
    web_contents->GetController().LoadURL(
        gurl, content::Referrer(), ui::PageTransition::PAGE_TRANSITION_TYPED,
        std::string());
    nav_observer.Wait();
    EXPECT_EQ(status == net::OK, nav_observer.last_navigation_succeeded());
    EXPECT_EQ(status, nav_observer.last_net_error_code());
  };

  auto random_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  GURL random_url =
      Extension::GetBaseURLFromExtensionId(random_guid).Resolve("ok.html");
  GURL dynamic_url = extension->dynamic_url().Resolve("ok.html");
  run(random_url, net::ERR_BLOCKED_BY_CLIENT);
  run(dynamic_url, net::OK);
}

// Web Accessible Resources.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest, FetchGuidFromFrame) {
  auto* extension = GetExtension();

  // Fetch url from frame to verify with expectations.
  auto test_frame_with_fetch = [&](const GURL& frame_url,
                                   const GURL& expected_frame_url,
                                   const GURL& fetch_url,
                                   const char* expected_fetch_url_contents) {
    SCOPED_TRACE(testing::Message() << "test_frame_with_fetch"
                                    << ": frame_url = " << frame_url
                                    << "; fetch_url = " << fetch_url);
    // Fetch and test resource.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), frame_url));
    content::WebContents* web_contents = GetActiveWebContents();
    EXPECT_EQ(expected_frame_url,
              web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

    constexpr char kFetchScriptTemplate[] =
        R"(
        fetch($1).then(result => {
          return result.text();
        }).catch(err => {
          return String(err);
        });)";
    EXPECT_EQ(
        expected_fetch_url_contents,
        content::EvalJs(web_contents, content::JsReplace(kFetchScriptTemplate,
                                                         fetch_url.spec())));
  };

  const struct {
    const char* title;
    GURL frame_url;
    GURL expected_frame_url;
    GURL fetch_url;
    const char* expected_fetch_url_contents;
  } test_cases[] = {
      {
          "Fetch web accessible resource from extension resource.",
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("web_accessible_resource.html"),
          "web_accessible_resource.html",
      },
      {
          "Fetch dynamic web accessible resource from extension resource.",
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("extension_resource.html"),
          extension->dynamic_url().Resolve("web_accessible_resource.html"),
          "web_accessible_resource.html",
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << test_case.title);
    test_frame_with_fetch(test_case.frame_url, test_case.expected_frame_url,
                          test_case.fetch_url,
                          test_case.expected_fetch_url_contents);
  }
}

}  // namespace

}  // namespace extensions
