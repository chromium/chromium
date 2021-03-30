// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kNonExistentUrlPath[] = "non-existent-url.html";
constexpr char kLoadFromDiskUrlPath[] = "telemetry_extension_test.html";
constexpr char kRegisteredUrlPath[] = "dpsl.js";
}  // namespace

class TelemetryExtensionIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  TelemetryExtensionIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kTelemetryExtension}, {});
  }

  web_app::SystemAppType GetTelemetryAppType() const {
    return web_app::SystemAppType::TELEMETRY;
  }

  content::WebContents* LaunchTelemetryApp() {
    // Important: wait until the telemetry app type be installed.
    WaitForTestSystemAppInstall();

    // Launch the Telemetry app.
    Browser* app_browser;
    return LaunchApp(GetTelemetryAppType(), &app_browser);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the TelemetryExtension app is successfully installed.
IN_PROC_BROWSER_TEST_P(TelemetryExtensionIntegrationTest,
                       TelemetryExtensionInstalled) {
  const GURL url(chromeos::kChromeUITelemetryExtensionURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(GetTelemetryAppType(), url,
                                                  "Telemetry Extension"));
}

// Tests that TelemetryExtensionUntrustedSource successfully loads the
// registered resource.
IN_PROC_BROWSER_TEST_P(
    TelemetryExtensionIntegrationTest,
    TelemetryExtensionUntrustedSourceCanLoadRegisteredResource) {
  content::WebContents* web_contents = LaunchTelemetryApp();

  const GURL registered_resource_gurl =
      GURL(std::string(chromeos::kChromeUIUntrustedTelemetryExtensionURL) +
           kRegisteredUrlPath);

  // Load the |registered_resource_gurl| into the same tab.
  // The |registered_resource_gurl| is a file that is included in the
  // TelemteryExtensionUntrustedSource's list of registered resources.
  EXPECT_TRUE(content::NavigateToURL(web_contents, registered_resource_gurl));
}

// A Test suite that use the switch "--telemetry-extension-dir".
class TelemetryExtensionWithDirIntegrationTest
    : public TelemetryExtensionIntegrationTest {
 public:
  TelemetryExtensionWithDirIntegrationTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);

    SystemWebAppManagerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionDirectory, src_dir.value());
  }
};

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    TelemetryExtensionIntegrationTest);

// Tests that TelemetryExtensionUntrustedSource can successfully load a resource
// from disk.
IN_PROC_BROWSER_TEST_P(TelemetryExtensionWithDirIntegrationTest,
                       TelemetryExtensionUntrustedSourceCanLoadFileFromDisk) {
  content::WebContents* web_contents = LaunchTelemetryApp();

  const GURL load_from_disk_resource_gurl =
      GURL(std::string(chromeos::kChromeUIUntrustedTelemetryExtensionURL) +
           kLoadFromDiskUrlPath);

  // Load the |load_from_disk_resource_gurl| into the same tab.
  // The |load_from_disk_resource_gurl| is a file that is not included in the
  // TelemteryExtensionUntrustedSource's list of registered resources.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents, load_from_disk_resource_gurl));

  // Verify that the file loaded from disk has the expected title.
  EXPECT_EQ(u"TelemetryExtension - Loaded From Disk", web_contents->GetTitle());
}

// Tests that TelemetryExtensionUntrustedSource fails to load non-existing
// chrome-untrusted://telemetry-extension/ resource from disk.
IN_PROC_BROWSER_TEST_P(
    TelemetryExtensionWithDirIntegrationTest,
    TelemetryExtensionUntrustedSourceFailToLoadNonExistentURL) {
  content::WebContents* web_contents = LaunchTelemetryApp();

  const GURL non_existent_resource_gurl =
      GURL(std::string(chromeos::kChromeUIUntrustedTelemetryExtensionURL) +
           kNonExistentUrlPath);

  EXPECT_FALSE(
      content::NavigateToURL(web_contents, non_existent_resource_gurl));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    TelemetryExtensionWithDirIntegrationTest);
