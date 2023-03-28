// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PendingApprovalTelemetryExtensionEventsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  PendingApprovalTelemetryExtensionEventsApiBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

 protected:
  std::string GetManifestFile(const std::string& matches_origin) override {
    return base::StringPrintf(R"(
      {
        "key": "%s",
        "name": "Test Telemetry Extension",
        "version": "1",
        "manifest_version": 3,
        "chromeos_system_extension": {},
        "background": {
          "service_worker": "sw.js"
        },
        "permissions": [
          "os.diagnostics",
          "os.events",
          "os.telemetry",
          "os.telemetry.serial_number",
          "os.telemetry.network_info"
        ],
        "externally_connectable": {
          "matches": [
            "%s"
          ]
        },
        "options_page": "options.html"
      }
    )",
                              public_key().c_str(), matches_origin.c_str());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       SmokeTest) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function checkDefinitionsExist() {
        chrome.test.assertTrue(chrome.os.events !== undefined);
        chrome.test.assertTrue(chrome.os.events.isEventSupported !== undefined);
        chrome.test.assertTrue(
          chrome.os.events.startCapturingEvents !== undefined);
        chrome.test.assertTrue(
          chrome.os.events.stopCapturingEvents !== undefined);
        chrome.test.assertTrue(
          chrome.os.events.onAudioJackEvent !== undefined);

        chrome.test.succeed();
      }
    ]);
    )");
}

}  // namespace chromeos
