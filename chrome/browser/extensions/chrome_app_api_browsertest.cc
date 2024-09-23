// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using extensions::Extension;

class ChromeAppAPITest : public extensions::ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool IsAppInstalledInMainFrame() {
    return IsAppInstalledInFrame(browser()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetPrimaryMainFrame());
  }
  bool IsAppInstalledInIFrame() {
    return IsAppInstalledInFrame(GetIFrame());
  }
  bool IsAppInstalledInFrame(content::RenderFrameHost* frame) {
    const char kGetAppIsInstalled[] = "window.chrome.app.isInstalled;";
    return content::EvalJs(frame, kGetAppIsInstalled).ExtractBool();
  }

  std::string InstallStateInMainFrame() {
    return InstallStateInFrame(browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetPrimaryMainFrame());
  }
  std::string InstallStateInIFrame() {
    return InstallStateInFrame(GetIFrame());
  }
  std::string InstallStateInFrame(content::RenderFrameHost* frame) {
    const char kGetAppInstallState[] =
        "new Promise(resolve => {"
        "    window.chrome.app.installState("
        "        function(s) { resolve(s); });"
        "});";
    return content::EvalJs(frame, kGetAppInstallState).ExtractString();
  }

  std::string RunningStateInMainFrame() {
    return RunningStateInFrame(browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetPrimaryMainFrame());
  }
  std::string RunningStateInIFrame() {
    return RunningStateInFrame(GetIFrame());
  }
  std::string RunningStateInFrame(content::RenderFrameHost* frame) {
    const char kGetAppRunningState[] = "window.chrome.app.runningState();";
    return content::EvalJs(frame, kGetAppRunningState).ExtractString();
  }

 private:
  content::RenderFrameHost* GetIFrame() {
    return content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameIsChildOfMainFrame));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, IsInstalled) {
  GURL app_url =
      embedded_test_server()->GetURL("app.com", "/extensions/test_file.html");
  GURL non_app_url = embedded_test_server()->GetURL(
      "nonapp.com", "/extensions/test_file.html");

  // Before the app is installed, app.com does not think that it is installed
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  // Even after the app is installed, the existing app.com tab is not in an
  // app process, so chrome.app.isInstalled should return false.
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Test that a non-app page has chrome.app.isInstalled = false.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_app_url));
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Test that a non-app page returns null for chrome.app.getDetails().
  const char kGetAppDetails[] =
      "JSON.stringify(window.chrome.app.getDetails());";
  EXPECT_EQ("null", content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        kGetAppDetails));

  // Check that an app page has chrome.app.isInstalled = true.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  EXPECT_TRUE(IsAppInstalledInMainFrame());

  // Check that an app page returns the correct result for
  // chrome.app.getDetails().
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  std::string result =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAppDetails)
          .ExtractString();
  std::optional<base::Value> result_value = base::JSONReader::Read(result);
  ASSERT_TRUE(result_value && result_value->is_dict());
  base::Value::Dict& app_details = result_value.value().GetDict();

  // extension->manifest() does not contain the id.
  app_details.Remove("id");
  EXPECT_EQ(app_details, *extension->manifest()->value());

  // Try to change app.isInstalled.  Should silently fail, so
  // that isInstalled should have the initial value.

  // Should not be able to alter window.chrome.app.isInstalled from javascript";
  EXPECT_EQ("true", content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "    (function() {"
                        "        var value = window.chrome.app.isInstalled;"
                        "        window.chrome.app.isInstalled = !value;"
                        "        if (window.chrome.app.isInstalled == value) {"
                        "            return 'true';"
                        "        } else {"
                        "            return 'false';"
                        "        }"
                        "    })()"));
}

// Test accessing app.isInstalled when the context has been invalidated (e.g.
// by removing the frame). Regression test for https://crbug.com/855853.
IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, IsInstalledFromRemovedFrame) {
  GURL app_url =
      embedded_test_server()->GetURL("app.com", "/extensions/test_file.html");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  constexpr char kScript[] =
      R"(var i = document.createElement('iframe');
         new Promise(resolve => {
           i.onload = function() {
             var frameApp = i.contentWindow.chrome.app;
             document.body.removeChild(i);
             var isInstalled = frameApp.isInstalled;
             resolve(isInstalled === undefined);
           };
           i.src = '%s';
           document.body.appendChild(i);
         });
         )";
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      base::StringPrintf(kScript, app_url.spec().c_str())));
}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningState) {
  GURL app_url = embedded_test_server()->GetURL(
      "app.com", "/extensions/get_app_details_for_frame.html");
  GURL non_app_url = embedded_test_server()->GetURL(
      "nonapp.com", "/extensions/get_app_details_for_frame.html");

  // Before the app is installed, app.com does not think that it is installed
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  EXPECT_EQ("not_installed", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("ready_to_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Reloading the page should put the tab in an app process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("running", RunningStateInMainFrame());
  EXPECT_TRUE(IsAppInstalledInMainFrame());

  // Disable the extension and verify the state.
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  service->DisableExtension(
      extension->id(),
      extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  EXPECT_EQ("disabled", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  service->EnableExtension(extension->id());
  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("ready_to_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // The non-app URL should still not be installed or running.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_app_url));

  EXPECT_EQ("not_installed", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  EXPECT_EQ("installed", InstallStateInIFrame());
  EXPECT_EQ("cannot_run", RunningStateInIFrame());

  // With --site-per-process, the iframe on nonapp.com will currently swap
  // processes and go into the hosted app process.
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(IsAppInstalledInIFrame());
  } else {
    EXPECT_FALSE(IsAppInstalledInIFrame());
  }
}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningStateFrame) {
  GURL app_url = embedded_test_server()->GetURL(
      "app.com", "/extensions/get_app_details_for_frame_reversed.html");

  // Check the install and running state of a non-app iframe running
  // within an app.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  EXPECT_EQ("not_installed", InstallStateInIFrame());
  EXPECT_EQ("cannot_run", RunningStateInIFrame());
  EXPECT_FALSE(IsAppInstalledInIFrame());
}

class ChromeAppAPIFencedFrameTest : public ChromeAppAPITest {
 public:
  ChromeAppAPIFencedFrameTest() {
    // kPrivacySandboxAdsAPIOverride must also be set since kFencedFrames
    // cannot be enabled independently without it.
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }

  ~ChromeAppAPIFencedFrameTest() override = default;

  void SetUpOnMainThread() override {
    ChromeAppAPITest::SetUpOnMainThread();
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server()->Start());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ChromeAppAPIFencedFrameTest, NoInfo) {
  GURL app_url = https_server()->GetURL(
      "a.test", "/extensions/get_app_details_for_fenced_frame.html");

  // Check the install and running state of a fenced frame running
  // within an app.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  auto render_frame_hosts = CollectAllRenderFrameHosts(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* fenced_frame = render_frame_hosts.at(1);
  ASSERT_TRUE(fenced_frame);
  EXPECT_EQ("cannot_run", RunningStateInFrame(fenced_frame));
}
