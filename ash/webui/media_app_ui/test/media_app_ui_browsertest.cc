// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"

#include "ash/webui/media_app_ui/media_app_guest_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_path.h"

namespace {

// File containing the test utility library, shared with integration tests.
constexpr base::FilePath::CharType kTestLibraryPath[] =
    FILE_PATH_LITERAL("ash/webui/system_apps/public/js/dom_testing_helpers.js");

// Test cases that run in the guest context.
constexpr char kGuestTestCases[] = "media_app_guest_ui_browsertest.js";

// Path to test files loaded via the TestFileRequestFilter.
constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/media_app_ui/test");

// Paths requested on the media-app origin that should be delivered by the test
// handler.
constexpr const char* kTestFiles[] = {
    kGuestTestCases,  "media_app_ui_browsertest.js", "driver.js",
    "test_worker.js", "guest_query_receiver.js",
};

}  // namespace

MediaAppUiBrowserTest::MediaAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(ash::kChromeUIMediaAppURL,
                                ash::kChromeUIMediaAppGuestURL,
                                {base::FilePath(kTestLibraryPath)},
                                kGuestTestCases) {
  ConfigureDefaultTestRequestHandler(
      base::FilePath(kTestFileLocation),
      {std::begin(kTestFiles), std::end(kTestFiles)});
}

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

// static
std::string MediaAppUiBrowserTest::AppJsTestLibrary() {
  return SandboxedWebUiAppTestBase::LoadJsTestLibrary(
      base::FilePath(kTestLibraryPath));
}
