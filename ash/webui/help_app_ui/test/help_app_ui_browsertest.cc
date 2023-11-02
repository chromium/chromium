// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/test/help_app_ui_browsertest.h"

#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"

// Path to test files loaded via the TestFileRequestFilter.
constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/help_app_ui/test");

// Test cases that run in the guest context.
constexpr char kGuestTestCases[] = "help_app_guest_ui_browsertest.js";

// Paths requested on the media-app origin that should be delivered by the test
// handler.
constexpr const char* kTestFiles[] = {
    kGuestTestCases,
    "help_app_ui_browsertest.js",
    "driver.js",
    "guest_query_receiver.js",
};

HelpAppUiBrowserTest::HelpAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(ash::kChromeUIHelpAppURL,
                                ash::kChromeUIHelpAppUntrustedURL,
                                {},
                                kGuestTestCases) {
  ConfigureDefaultTestRequestHandler(
      base::FilePath(kTestFileLocation),
      {std::begin(kTestFiles), std::end(kTestFiles)});
}

HelpAppUiBrowserTest::~HelpAppUiBrowserTest() = default;
