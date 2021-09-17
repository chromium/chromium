// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

namespace {

constexpr char kChromeOSSystemExtensionId[] =
    "gogonhoemckpdpadfnjnpgbjpbjnodgc";
constexpr char kNormalExtensionId[] = "adjjaddigjeahomddneilolepekgojjk";

}  // namespace

using TelemetryExtensionBrowserTest = BaseTelemetryExtensionBrowserTest;

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest,
                       CanReceiveMessageExternal) {
  // Start listening on the receiving extension.
  ExtensionTestMessageListener listener(/*will_reply=*/false);
  listener.set_extension_id(kChromeOSSystemExtensionId);

  // Must be outlive the extension.
  extensions::TestExtensionDir test_dir_receiver;

  // Load and run the receiving extenion (chromeos_system_extension).
  const extensions::Extension* receiver = LoadExtensionWithServiceWorker(
      test_dir_receiver, base::StringPrintf(R"(
        chrome.test.runTests([
          function runtimeOnMessageExternal() {
            chrome.runtime.onMessageExternal.addListener(
              (message, sender, sendResponse) => {
                chrome.test.assertEq("%s", sender.id);
                chrome.test.assertEq("ping", message);
                sendResponse("pong");
            });
            chrome.test.sendMessage('ready');
          }
        ]);
      )", kNormalExtensionId));
  ASSERT_TRUE(receiver);

  // Make sure the receiving extension is loaded first before loading the
  // sending extension.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("ready", listener.message());

  // Start listening on the sending extension.
  listener.Reset();
  listener.set_extension_id(kNormalExtensionId);

  // Must be outlive the extension.
  extensions::TestExtensionDir test_dir_sender;

  // Load and run the sending extension (normal extension).
  const extensions::Extension* sender =
      LoadExtensionWithManifestAndServiceWorker(
          test_dir_sender,
          R"(
            {
              // normal extension (id=adjjaddigjeahomddneilolepekgojjk)
              "key": "MMIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAt2CwI94nqAQzLTBHSIwtkMlkoRyhu27rmkDsBneMprscOzl4524Y0bEA+0RSjNZB+kZgP6M8QAZQJHCpAzULXa49MooDDIdzzmqQswswguAALm2FS7XP2N0p2UYQneQce4Wehq/C5Yr65mxasAgjXgDWlYBwV3mgiISDPXI/5WG94TM2Z3PDQePJ91msDAjN08jdBme3hAN976yH3Q44M7cP1r+OWRkZGwMA6TSQjeESEuBbkimaLgPIyzlJp2k6jwuorG5ujmbAGFiTQoSDFBFCjwPEtywUMLKcZjH4fD76pcIQIdkuuzRQCVyuImsGzm1cmGosJ/Z4iyb80c1ytwIDAQAB",
              "name": "Test Extension",
              "version": "1",
              "manifest_version": 3,
              "background": {
                "service_worker": "sw.js"
              }
            }
          )",
          base::StringPrintf(R"(
            chrome.test.runTests([
              function runtimeSendMessage() {
                // send a message to chromeos_system_extension
                chrome.runtime.sendMessage("%s", "ping",
                  (result) => {
                    chrome.test.assertEq("pong", result);
                    chrome.test.sendMessage('success');
                });
              }
            ]);
          )", kChromeOSSystemExtensionId));
  ASSERT_TRUE(sender);

  // Now wait until the sending extension receives a reply.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

}  // namespace chromeos
