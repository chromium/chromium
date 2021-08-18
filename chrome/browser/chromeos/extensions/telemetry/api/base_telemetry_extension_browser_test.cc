// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"

#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

BaseTelemetryExtensionBrowserTest::BaseTelemetryExtensionBrowserTest() =
    default;
BaseTelemetryExtensionBrowserTest::~BaseTelemetryExtensionBrowserTest() =
    default;

void BaseTelemetryExtensionBrowserTest::CreateExtensionAndRunServiceWorker(
    const std::string& service_worker_content) {
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
      {
        // Sample telemetry extension public key. Currently, this is the only
        // allowed extension to declare "chromeos_system_extension" key.
        // See //chrome/common/chromeos/extensions/api/_manifest_features.json
        "key": "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAt2CwI94nqAQzLTBHSIwtkMlkoRyhu27rmkDsBneMprscOzl4524Y0bEA+0RSjNZB+kZgP6M8QAZQJHCpAzULXa49MooDDIdzzmqQswswguAALm2FS7XP2N0p2UYQneQce4Wehq/C5Yr65mxasAgjXgDWlYBwV3mgiISDPXI/5WG94TM2Z3PDQePJ91msDAjN08jdBme3hAN976yH3Q44M7cP1r+OWRkZGwMA6TSQjeESEuBbkimaLgPIyzlJp2k6jwuorG5ujmbAGFiTQoSDFBFCjwPEtywUMLKcZjH4fD76pcIQIdkuuzRQCVyuImsGzm1cmGosJ/Z4iyb80c1ytwIDAQAB",
        "name": "Test Telemetry Extension",
        "version": "1",
        "manifest_version": 3,
        "chromeos_system_extension": {},
        "background": {
          "service_worker": "sw.js"
        }
      }
    )");
  test_dir.WriteFile(FILE_PATH_LITERAL("sw.js"), service_worker_content);

  // Must be initialised before loading extension.
  extensions::ResultCatcher result_catcher;

  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace chromeos
