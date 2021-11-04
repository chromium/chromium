// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_hardware_info_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

ExtensionInfoTestParams::ExtensionInfoTestParams(
    const std::string& extension_id,
    const std::string& public_key,
    const std::string& pwa_page_url,
    const std::string& matches_origin)
    : extension_id(extension_id),
      public_key(public_key),
      pwa_page_url(pwa_page_url),
      matches_origin(matches_origin) {}
ExtensionInfoTestParams::ExtensionInfoTestParams(
    const ExtensionInfoTestParams& other) = default;
ExtensionInfoTestParams::~ExtensionInfoTestParams() = default;

// static
const std::vector<ExtensionInfoTestParams>
    BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams{
        ExtensionInfoTestParams(
            /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
            /*public_key=*/
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAt2CwI94nqAQzLTBHSIwtkMlkoRyhu27rmkDsBneMprscOzl4524Y0bEA+0RSjNZB+kZgP6M8QAZQJHCpAzULXa49MooDDIdzzmqQswswguAALm2FS7XP2N0p2UYQneQce4Wehq/C5Yr65mxasAgjXgDWlYBwV3mgiISDPXI/5WG94TM2Z3PDQePJ91msDAjN08jdBme3hAN976yH3Q44M7cP1r+OWRkZGwMA6TSQjeESEuBbkimaLgPIyzlJp2k6jwuorG5ujmbAGFiTQoSDFBFCjwPEtywUMLKcZjH4fD76pcIQIdkuuzRQCVyuImsGzm1cmGosJ/Z4iyb80c1ytwIDAQAB",
            /*pwa_page_url=*/"http://www.google.com",
            /*matches_origin=*/"*://www.google.com/*"),
        ExtensionInfoTestParams(
            /*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
            /*public_key=*/
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAm6NnMxmC5iaSFAILkuIkGXllW1Tie3AW+7Ty3R3sbQ7EVNG3HtFIG7jbJIvSko+lrTa1U1VveOXZw1u3y1T49ihR2XFU0w6+3OAXzjuUimKUviGao6EN4KfCegtKyDQnMw0zATBisqBxrPLzGBXxP/AhxH2OGgyyioVOzoCF+rnBY7ed+Wh+mPI7s9lrECeisUHHM5xbHXXgr8bnvt3U27jnsctwJWKHfcbd3rpMJwBfOmPfuQ0MZvySVkTr/WYeemkwR8/4mek9/UIGMB8X+mXdU9OV/qhylqy6FzRw/FdV+RcmzAwEgNmhgXP7TwtFBsUdtTIe2Kio26ciK7PSKwIDAQAB",
            /*pwa_page_url=*/"http://hpcs-appschr.hpcloud.hp.com",
            /*matches_origin=*/"*://hpcs-appschr.hpcloud.hp.com/*")};

BaseTelemetryExtensionBrowserTest::BaseTelemetryExtensionBrowserTest() =
    default;
BaseTelemetryExtensionBrowserTest::~BaseTelemetryExtensionBrowserTest() =
    default;

void BaseTelemetryExtensionBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  // Make sure that current user is a device owner.
  auto* const user_manager =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  user_manager->SetOwnerId(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());

  // Make sure device OEM is allowlisted.
  hardware_info_delegate_factory_ =
      std::make_unique<FakeHardwareInfoDelegate::Factory>("HP\n");
  HardwareInfoDelegate::Factory::SetForTesting(
      hardware_info_delegate_factory_.get());

  if (should_open_pwa_ui_) {
    host_resolver()->AddRule("*", "127.0.0.1");
    // Make sure PWA UI is open.
    pwa_page_rfh_ =
        ui_test_utils::NavigateToURL(browser(), GURL(GetParam().pwa_page_url));
    ASSERT_TRUE(pwa_page_rfh_);
  }
}

void BaseTelemetryExtensionBrowserTest::CreateExtensionAndRunServiceWorker(
    const std::string& service_worker_content) {
  // Must outlive the extension.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(
      GetManifestFile(GetParam().public_key, GetParam().matches_origin));
  test_dir.WriteFile(FILE_PATH_LITERAL("sw.js"), service_worker_content);
  test_dir.WriteFile(FILE_PATH_LITERAL("options.html"), "");

  // Must be initialised before loading extension.
  extensions::ResultCatcher result_catcher;

  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

std::string BaseTelemetryExtensionBrowserTest::GetManifestFile(
    const std::string& public_key,
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
          "os.telemetry.serial_number"
        ],
        "externally_connectable": {
          "matches": [
            "%s"
          ]
        },
        "options_page": "options.html"
      }
    )", public_key.c_str(), matches_origin.c_str());
}

TEST(BaseTelemetryExtensionBrowserTest, VerifyAllExtensionInfoTestParams) {
  ASSERT_EQ(
      2, BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams.size());

  // Verifies that all allowlisted extension ids are covered with test params.
  ASSERT_EQ(GetChromeOSSystemExtensionInfosSize(),
    BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams.size());
}

}  // namespace chromeos
