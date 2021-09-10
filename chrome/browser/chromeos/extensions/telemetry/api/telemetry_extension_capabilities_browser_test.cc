// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

using TelemetryExtensionBrowserTest = BaseTelemetryExtensionBrowserTest;

// Tests that chromeos_system_extension is able to define externally_connectable
// manifest key and receive messages from another extension.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest,
                       CanReceiveMessageExternal) {
  // Must be initialised before loading extension.
  extensions::ResultCatcher result_catcher;

  const extensions::Extension* receiver = LoadExtensionWithServiceWorker(R"(
    chrome.test.runTests([
      async function runtimeOnMessageExternal() {
        chrome.runtime.onMessageExternal.addListener(
          ((message, sender, sendResponse) => {
            chrome.test.assertEq("adjjaddigjeahomddneilolepekgojjk", sender.id);
            chrome.test.assertEq("ping", message);
            sendResponse("pong");
        }));
      }
    ]);
  )");
  ASSERT_TRUE(receiver);

  const extensions::Extension* sender =
    LoadExtensionWithManifestAndServiceWorker(R"(
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
    R"(chrome.test.runTests([
      async function runtimeSendMessage() {
        // send a message to chromeos_system_extension
        chrome.runtime.sendMessage("gogonhoemckpdpadfnjnpgbjpbjnodgc", "ping",
          (result) => {
            chrome.test.assertEq("pong", result);
            chrome.test.succeed();
        });
      }
    ]);
  )");
  ASSERT_TRUE(sender);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace chromeos
