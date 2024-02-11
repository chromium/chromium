// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/test/help_app_ui_browsertest.h"

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "content/public/test/browser_test.h"

// Path to test files loaded via the TestFileRequestFilter.
constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/help_app_ui/resources");

// Test cases that run in the guest (untrusted) context.
constexpr char kGuestTestCases[] = "help_app_guest_ui_browsertest.js";

// Test cases that run in the host (trusted) context.
constexpr char kTestHarness[] = "help_app_ui_browsertest.js";

// Paths requested on the media-app origin that should be delivered by the test
// handler.
constexpr const char* kTestFiles[] = {
    kGuestTestCases,
    kTestHarness,
    "driver.js",
    "guest_query_receiver.js",
};

HelpAppUiBrowserTest::HelpAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(ash::kChromeUIHelpAppURL,
                                ash::kChromeUIHelpAppUntrustedURL,
                                {},
                                kGuestTestCases,
                                kTestHarness) {
  ConfigureDefaultTestRequestHandler(
      base::FilePath(kTestFileLocation),
      {std::begin(kTestFiles), std::end(kTestFiles)});
}

HelpAppUiBrowserTest::~HelpAppUiBrowserTest() = default;

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, HasChromeSchemeURL) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, HasTitleAndLang) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest,
                       BodyHasCorrectBackgroundColorInDarkMode) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  RunCurrentTest();
}

// Test cases injected into the guest context.
// See implementations in `help_app_guest_ui_browsertest.js`.

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestHasLang) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestLoadsLoadTimeData) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestCanSearchWithHeadings) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestCanSearchWithCategories) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestCanClearSearchIndex) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(HelpAppUiBrowserTest, GuestCanGetDeviceInfo) {
  RunCurrentTest("runTestInGuest");
}
