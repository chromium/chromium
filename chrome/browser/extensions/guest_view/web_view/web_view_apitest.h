// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GUEST_VIEW_WEB_VIEW_WEB_VIEW_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_GUEST_VIEW_WEB_VIEW_WEB_VIEW_APITEST_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "ui/gfx/switches.h"

// This test class is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Base class for WebView tests.
class WebViewAPITest : public ExtensionBrowserTest {
 protected:
  WebViewAPITest();

  // Launches the app in `app_location`.
  void LaunchApp(const std::string& app_location);

  // Closes all app windows.
  void CloseAppWindows();

  // Runs the test `test_name` in `app_location`. RunTest will launch the app
  // and execute the javascript function runTest(test_name) inside the app.
  // If `ad_hoc_framework` is true, the test app defines its own testing
  // framework, otherwise the test app uses the chrome.test framework.
  // See https://crbug.com/40590323
  void RunTest(const std::string& test_name,
               const std::string& app_location,
               bool ad_hoc_framework = true);

  // Starts/Stops the embedded test server.
  void StartTestServer(const std::string& app_location);
  void StopTestServer();

  content::WebContents* GetEmbedderWebContents();

  // Returns the GuestViewManager singleton.
  guest_view::TestGuestViewManager* GetGuestViewManager();

  void SendMessageToEmbedder(const std::string& message);

  // content::BrowserTestBase implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      embedder_web_contents_;
  guest_view::TestGuestViewManagerFactory factory_;
  base::DictValue test_config_;

 private:
  content::WebContents* GetFirstAppWindowWebContents();
};

class WebViewDPIAPITest : public WebViewAPITest {
 protected:
  void SetUp() override;
  static float scale() { return 2.0f; }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GUEST_VIEW_WEB_VIEW_WEB_VIEW_APITEST_H_
