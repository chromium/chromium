// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class PackExtensionOnStartupBrowserTest : public InProcessBrowserTest {
 public:
  PackExtensionOnStartupBrowserTest() = default;
  ~PackExtensionOnStartupBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    test_extension_dir_ = std::make_unique<TestExtensionDir>();
    // Create an extension with some permissions that are guarded by
    // base::Features.
    // Note: Unfortunately, this is bound to become out-of-date. Some
    // of these features are currently restricted by base::Features, but we'll
    // eventually remove those restrictions. There's no good workaround for this
    // that *also* allows us to do the early initialization required in this
    // test.
    static constexpr char kManifest[] =
        R"({
             "name": "Test extension",
             "version": "0.1",
             "manifest_version": 3,
             "host_permissions": ["*://example.com/*"],
             "permissions": ["storage", "tabs", "userScripts", "debugger"],
             "background": {"service_worker": "background.js"}
           })";
    test_extension_dir_->WriteManifest(kManifest);
    test_extension_dir_->WriteFile(FILE_PATH_LITERAL("background.js"),
                                   "// blank");

    // Append the switch to pack the extension.
    command_line->AppendSwitchASCII(
        ::switches::kPackExtension,
        test_extension_dir_->UnpackedPath().AsUTF8Unsafe());

    // Packing extensions has a different exit code.
    set_expected_exit_code(
        chrome::RESULT_CODE_NORMAL_EXIT_PACK_EXTENSION_SUCCESS);
  }

 protected:
  std::unique_ptr<TestExtensionDir> test_extension_dir_;
};

// Tests that appending the --pack-extension switch on startup succeeds with
// a "real" browser (i.e., outside of unit tests).
// Regression test for https://crbug.com/1498558.
IN_PROC_BROWSER_TEST_F(PackExtensionOnStartupBrowserTest,
                       PackExtensionOnStartup) {
  // Interesting case: because the --pack-extension switch results in Chrome
  // immediately exiting, this test is effectively entirely tested between the
  // SetUpCommandLine() method and when Chrome starts. This test body is never
  // reached. That's okay -- the test still serves its purpose and *does*
  // properly exercise this scenario, including checking the exit code from the
  // browser -- but it means we can't put any logic here.
}

}  // namespace extensions
