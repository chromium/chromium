// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

namespace {

constexpr char kChromeOSSystemExtensionId[] =
    "gogonhoemckpdpadfnjnpgbjpbjnodgc";

}  // namespace

using TelemetryExtensionBrowserTest = BaseTelemetryExtensionBrowserTest;

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest,
                       CanReceiveMessageExternal) {
  // Start listening on the extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);

  // Must outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;

  // Load and run the extenion (chromeos_system_extension).
  const extensions::Extension* receiver = LoadExtensionWithServiceWorker(
      test_dir_receiver, base::StringPrintf(R"(
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
      )", kPwaPageUrlString));
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
      kChromeOSSystemExtensionId);
  pwa_page_rfh_->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script),
                                           base::NullCallback());

  // Wait until the extension receives the message.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

}  // namespace chromeos
