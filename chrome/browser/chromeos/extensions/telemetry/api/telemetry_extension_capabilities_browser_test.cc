// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

using TelemetryExtensionBrowserTest = BaseTelemetryExtensionBrowserTest;

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_P(TelemetryExtensionBrowserTest,
                       CanReceiveMessageExternal) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;
  test_dir_receiver.WriteManifest(
      GetManifestFile(GetParam().public_key, GetParam().matches_origin));
  test_dir_receiver.WriteFile(FILE_PATH_LITERAL("options.html"), "");
  test_dir_receiver.WriteFile("sw.js", base::StringPrintf(R"(
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
      )", GetParam().pwa_page_url.c_str()));

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
      GetParam().extension_id.c_str());
  pwa_page_rfh_->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script),
                                           base::NullCallback());

  // Wait until the extension receives the message.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

// Tests that chromeos_system_extension is able to define options_page manifest
// key and user can navigate to the options page.
IN_PROC_BROWSER_TEST_P(TelemetryExtensionBrowserTest,
                       CanNavigateToOptionsPage) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(
      GetManifestFile(GetParam().public_key, GetParam().matches_origin));
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

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionBrowserTest,
    testing::ValuesIn(
        BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams));

}  // namespace chromeos
