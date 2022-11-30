// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_api_guard_delegate.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

BaseTelemetryExtensionBrowserTest::BaseTelemetryExtensionBrowserTest() =
    default;
BaseTelemetryExtensionBrowserTest::~BaseTelemetryExtensionBrowserTest() =
    default;

void BaseTelemetryExtensionBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  // Make sure ApiGuardDelegate::CanAccessApi() returns an empty error message.
  api_guard_delegate_factory_ = std::make_unique<FakeApiGuardDelegate::Factory>(
      /*error_message=*/"");
  ApiGuardDelegate::Factory::SetForTesting(api_guard_delegate_factory_.get());
}

void BaseTelemetryExtensionBrowserTest::CreateExtensionAndRunServiceWorker(
    const std::string& service_worker_content) {
  // Must outlive the extension.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(GetManifestFile(matches_origin()));
  test_dir.WriteFile(FILE_PATH_LITERAL("sw.js"), service_worker_content);
  test_dir.WriteFile(FILE_PATH_LITERAL("options.html"), "");

  // Must be initialised before loading extension.
  extensions::ResultCatcher result_catcher;

  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

std::string BaseTelemetryExtensionBrowserTest::GetManifestFile(
    const std::string& matches_origin) {
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

std::string BaseTelemetryExtensionBrowserTest::extension_id() const {
  return "gogonhoemckpdpadfnjnpgbjpbjnodgc";
}

std::string BaseTelemetryExtensionBrowserTest::public_key() const {
  return "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAt2CwI94nqAQzLTBHSIwtkMlkoRyhu27rmkDsBneMprscOzl4524Y0bEA+0RSjNZB+kZgP6M8QAZQJHCpAzULXa49MooDDIdzzmqQswswguAALm2FS7XP2N0p2UYQneQce4Wehq/C5Yr65mxasAgjXgDWlYBwV3mgiISDPXI/5WG94TM2Z3PDQePJ91msDAjN08jdBme3hAN976yH3Q44M7cP1r+OWRkZGwMA6TSQjeESEuBbkimaLgPIyzlJp2k6jwuorG5ujmbAGFiTQoSDFBFCjwPEtywUMLKcZjH4fD76pcIQIdkuuzRQCVyuImsGzm1cmGosJ/Z4iyb80c1ytwIDAQAB";
}

std::string BaseTelemetryExtensionBrowserTest::pwa_page_url() const {
  return "http://googlechromelabs.github.io";
}

std::string BaseTelemetryExtensionBrowserTest::matches_origin() const {
  return "*://googlechromelabs.github.io/*";
}

}  // namespace chromeos
