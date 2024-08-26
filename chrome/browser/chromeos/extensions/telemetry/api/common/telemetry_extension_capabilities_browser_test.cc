// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

class TelemetryExtensionCapabilitiesBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionCapabilitiesBrowserTest() = default;
  TelemetryExtensionCapabilitiesBrowserTest(
      const TelemetryExtensionCapabilitiesBrowserTest&) = delete;
  TelemetryExtensionCapabilitiesBrowserTest& operator=(
      const TelemetryExtensionCapabilitiesBrowserTest&) = delete;
  ~TelemetryExtensionCapabilitiesBrowserTest() override = default;

  GURL GetPwaGURL() const {
    return embedded_test_server()->GetURL("/simple.html");
  }

  // BaseTelemetryExtensionBrowserTest overrides:
  std::string pwa_page_url() const override { return GetPwaGURL().spec(); }
  std::string matches_origin() const override { return GetPwaGURL().spec(); }

  // BaseTelemetryExtensionBrowserTest override:
  void SetUp() override {
    // setup the test server
    embedded_test_server()->ServeFilesFromDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    BaseTelemetryExtensionBrowserTest::SetUp();
  }

  // BaseTelemetryExtensionBrowserTest override:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BaseTelemetryExtensionBrowserTest::SetUpCommandLine(command_line);

    // Make sure the PWA origin is allowed.
    command_line->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionPwaOriginOverrideForTesting,
        pwa_page_url());
  }

  // BaseTelemetryExtensionBrowserTest override:
  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();

    // This is needed when navigating to a network URL (e.g.
    // ui_test_utils::NavigateToURL). Rules can only be added before
    // BrowserTestBase::InitializeNetworkProcess() is called because host
    // changes will be disabled afterwards.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->StartAcceptingConnections();
  }
};

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionCapabilitiesBrowserTest,
                       CanReceiveMessageExternal) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener;

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;
  test_dir_receiver.WriteManifest(
      GetManifestFile(public_key(), matches_origin()));
  test_dir_receiver.WriteFile(FILE_PATH_LITERAL("options.html"), "");
  test_dir_receiver.WriteFile("sw.js",
                              base::StringPrintf(R"(
        chrome.test.runTests([
          function runtimeOnMessageExternal() {
            chrome.runtime.onMessageExternal.addListener(
              (message, sender, sendResponse) => {
                chrome.test.assertEq("%s", sender.url);
                chrome.test.assertEq("ping", message);
                chrome.test.sendMessage('success');
            });
            chrome.test.sendMessage('ready');
          }
        ]);
      )",
                                                 pwa_page_url().c_str()));

  // Load and run the extenion (chromeos_system_extension).
  const extensions::Extension* receiver =
      LoadExtension(test_dir_receiver.UnpackedPath());
  ASSERT_TRUE(receiver);

  // Make sure the extension is ready.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("ready", listener.message());

  listener.Reset();

  // From the |kPwaPageUrlString| page, send a message to
  // |kChromeOSSystemExtensionId|.
  // Note: |pwa_page_rfh_| is the RenderFrameHost for |kPwaPageUrlString| page.
  const auto script = base::StringPrintf(
      "window.chrome.runtime.sendMessage('%s', 'ping', (result) => {});",
      extension_id().c_str());

  auto* pwa_page_rfh = ui_test_utils::NavigateToURL(browser(), GetPwaGURL());
  ASSERT_TRUE(pwa_page_rfh);
  pwa_page_rfh->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script),
                                          base::NullCallback(),
                                          content::ISOLATED_WORLD_ID_GLOBAL);

  // Wait until the extension receives the message.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

// Tests that chromeos_system_extension is able to define options_page manifest
// key and user can navigate to the options page.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionCapabilitiesBrowserTest,
                       CanNavigateToOptionsPage) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener;

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(GetManifestFile(public_key(), matches_origin()));
  test_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                     "<script>chrome.test.sendMessage('done')</script>");
  test_dir.WriteFile("sw.js", "chrome.test.sendMessage('ready');");

  // Load and run the extenion (chromeos_system_extension).
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Make sure the extension is ready.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("ready", listener.message());

  listener.Reset();

  // Navigate to the extension's options page.
  ASSERT_TRUE(extensions::OptionsPageInfo::HasOptionsPage(extension));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extensions::OptionsPageInfo::GetOptionsPage(extension)));

  // Wait until the extension's options page is loaded.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("done", listener.message());
}

}  // namespace chromeos
