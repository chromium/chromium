// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
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

  // BaseTelemetryExtensionBrowserTest:
  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();

    // This is needed when navigating to a network URL (e.g.
    // ui_test_utils::NavigateToURL). Rules can only be added before
    // BrowserTestBase::InitializeNetworkProcess() is called because host
    // changes will be disabled afterwards.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionCapabilitiesBrowserTest,
                       CanReceiveMessageExternal) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;
  test_dir_receiver.WriteManifest(GetManifestFile(matches_origin()));
  test_dir_receiver.WriteFile(FILE_PATH_LITERAL("options.html"), "");
  test_dir_receiver.WriteFile("sw.js",
                              base::StringPrintf(R"(
        chrome.test.runTests([
          function runtimeOnMessageExternal() {
            chrome.runtime.onMessageExternal.addListener(
              (message, sender, sendResponse) => {
                chrome.test.assertEq("%s", sender.origin);
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

  auto* pwa_page_rfh =
      ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url()));
  ASSERT_TRUE(pwa_page_rfh);
  pwa_page_rfh->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script),
                                          base::NullCallback());

  // Wait until the extension receives the message.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

// Tests that chromeos_system_extension is able to define options_page manifest
// key and user can navigate to the options page.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionCapabilitiesBrowserTest,
                       CanNavigateToOptionsPage) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(GetManifestFile(matches_origin()));
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

namespace {

constexpr char kPwaOriginOverride[] = "*://pwa.website.com/*";
constexpr char kPwaPageUrl[] = "http://pwa.website.com";

}  // namespace

class TelemetryExtensionCapabilitiesWithCmdBrowserTest
    : public TelemetryExtensionCapabilitiesBrowserTest {
 public:
  TelemetryExtensionCapabilitiesWithCmdBrowserTest() = default;
  TelemetryExtensionCapabilitiesWithCmdBrowserTest(
      const TelemetryExtensionCapabilitiesWithCmdBrowserTest&) = delete;
  TelemetryExtensionCapabilitiesWithCmdBrowserTest& operator=(
      const TelemetryExtensionCapabilitiesWithCmdBrowserTest&) = delete;
  ~TelemetryExtensionCapabilitiesWithCmdBrowserTest() override = default;

  // extensions::ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionPwaOriginOverrideForTesting,
        kPwaOriginOverride);
  }
};

// Tests that the extension's PWA origin is overridden in tests using the
// command line switch |kTelemetryExtensionPwaOriginOverrideForTesting|. The
// test also makes sure the command line switch is copied across processes.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionCapabilitiesWithCmdBrowserTest,
                       CanOverridePwaOriginForTesting) {
  // Make sure the PWA origin is overridden.
  const auto extension_info = GetChromeOSExtensionInfoForId(extension_id());
  EXPECT_EQ(kPwaOriginOverride, extension_info.pwa_origin);

  // Open the PWA page url to bypass IsPwaUiOpen() check.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kPwaPageUrl)));

  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;
  test_dir_receiver.WriteManifest(GetManifestFile(kPwaOriginOverride));
  test_dir_receiver.WriteFile(FILE_PATH_LITERAL("options.html"), "");
  test_dir_receiver.WriteFile("sw.js", R"(
    chrome.test.runTests([
      // Choose a candidate API function that doesn't require any setup.
      async function runBatteryHealthRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryHealthRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.sendMessage('ready');
      }
    ]);
  )");

  // Load and run the extenion (chromeos_system_extension). If the extension has
  // been installed and run, then the command line switch must have been copied
  // across processes.
  const extensions::Extension* receiver =
      LoadExtension(test_dir_receiver.UnpackedPath());
  ASSERT_TRUE(receiver);

  // Make sure the sw.js was being run.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("ready", listener.message());
}

}  // namespace chromeos
