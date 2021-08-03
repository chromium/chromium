// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api.h"

#include <string>
#include <utility>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TelemetryExtensionBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  TelemetryExtensionBrowserTest() {}
  ~TelemetryExtensionBrowserTest() override {}

  TelemetryExtensionBrowserTest(const TelemetryExtensionBrowserTest&) = delete;
  TelemetryExtensionBrowserTest& operator=(
      const TelemetryExtensionBrowserTest&) = delete;

 protected:
  void CreateExtensionAndRunServiceWorker(
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
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest, GetVpdInfoError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest, GetVpdInfoSuccess) {
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto os_version = cros_healthd::mojom::OsVersion::New();

      auto system_info = cros_healthd::mojom::SystemInfo::New();
      system_info->first_power_date = "2021-50";
      system_info->product_model_name = "COOL-LAPTOP-CHROME";
      system_info->product_serial_number = "5CD9132880";
      system_info->product_sku_number = "sku15";
      system_info->os_version = std::move(os_version);

      telemetry_info->system_result =
          cros_healthd::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthdClient::Get());

    cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        const result = await chrome.os.telemetry.getVpdInfo();
        chrome.test.assertEq("2021-50", result.activateDate);
        chrome.test.assertEq("COOL-LAPTOP-CHROME", result.modelName);
        chrome.test.assertEq("5CD9132880", result.serialNumber);
        chrome.test.assertEq("sku15", result.skuNumber);
        chrome.test.succeed();
      }
    ]);
  )");
}

namespace {

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void GetLog(const std::string& log_name,
              DBusMethodCallback<std::string> callback) override {
    EXPECT_EQ(log_name, "oemdata");
    std::move(callback).Run(absl::nullopt);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest, GetOemDataError) {
  DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
      std::make_unique<TestDebugDaemonClient>());

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionBrowserTest, GetOemDataSuccess) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        const result = await chrome.os.telemetry.getOemData();
        chrome.test.assertEq(
          "oemdata: response from GetLog", result.oemData);
        chrome.test.succeed();
      }
    ]);
  )");
}

}  // namespace chromeos
