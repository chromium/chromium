// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/print_management_delegate_impl.h"

#include <memory>

#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::print_management {

namespace {

constexpr char kOsPrinterSettingsUrl[] = "chrome://os-settings/cupsPrinters";

const GURL& GetActiveUrl(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

}  // namespace

class PrintManagementDelegateImplTest
    : public ash::SystemWebAppIntegrationTest {
 public:
  PrintManagementDelegateImplTest() = default;
  ~PrintManagementDelegateImplTest() override = default;
};

// Verify OS Settings is opened to the expected page when Launch Printer
// settings function called.
IN_PROC_BROWSER_TEST_P(PrintManagementDelegateImplTest, LaunchPrinterSettings) {
  // Ensure System apps installed.
  WaitForTestSystemAppInstall();

  // Setup navigation observer.
  GURL os_settings_printer(kOsPrinterSettingsUrl);
  content::TestNavigationObserver navigation_observer(os_settings_printer);
  navigation_observer.StartWatchingNewWebContents();

  // Attempt to launch Printer settings from delegate.
  ash::print_management::PrintManagementDelegateImpl delegate;
  delegate.LaunchPrinterSettings();

  // Wait for OS Settings to open.
  navigation_observer.Wait();

  // Verify correct OS Settings page is opened.
  Browser* settings_browser =
      ash::FindSystemWebAppBrowser(profile(), SystemWebAppType::SETTINGS);
  ASSERT_NE(nullptr, settings_browser);
  ASSERT_EQ(os_settings_printer, GetActiveUrl(settings_browser));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PrintManagementDelegateImplTest);

}  // namespace ash::print_management
