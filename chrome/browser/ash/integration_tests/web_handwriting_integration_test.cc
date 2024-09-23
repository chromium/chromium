// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chromeos/crosier/annotations.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/services/machine_learning/public/cpp/ml_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

// TODO(jamescook): Support Lacros. This will require crosapi to be bootstrapped
// for Lacros Crosier tests.
class WebHandwritingIntegrationTest : public MixinBasedInProcessBrowserTest {
 public:
  WebHandwritingIntegrationTest()
      : supports_ondevice_handwriting_(crosier::HasRequirement(
            crosier::Requirement::kOndeviceHandwriting)) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    if (supports_ondevice_handwriting_) {
      command_line->AppendSwitchASCII(::switches::kOndeviceHandwritingSwitch,
                                      "use_rootfs");
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TearDownOnMainThread() override {
    // Close the browser otherwise the test may hang on shutdown.
    browser()->window()->Close();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ChromeOSIntegrationTestMixin chromeos_mixin_{&mixin_host_};

  // Whether this board supports on-device handwriting.
  bool supports_ondevice_handwriting_ = false;
};

IN_PROC_BROWSER_TEST_F(WebHandwritingIntegrationTest, Recognition) {
  // The full board name may have the form "glimmer-signed-mp-v4keys" and we
  // just want "glimmer".
  std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board.empty()) {
    LOG(ERROR) << "Unable to determine LSB release board";
    GTEST_SKIP();
  }
  // TODO(b/342174514): Fails on jacuzzi.
  if (board[0] == "jacuzzi") {
    GTEST_SKIP();
  }

  // Navigate to the appropriate test page, based on whether handwriting
  // recognition is supported or not.
  const char* test_file =
      supports_ondevice_handwriting_
          ? "web_handwriting_recognition.html"
          : "web_handwriting_recognition_not_supported.html";
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath("chromeos/web_handwriting/"), base::FilePath(test_file));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for JS object window.resultPromise to exist in the test page.
  bool object_exists = false;
  int iterations = 0;
  while (iterations < 10 && !object_exists) {
    object_exists = content::EvalJs(web_contents, "'resultPromise' in window")
                        .ExtractBool();
    if (object_exists) {
      break;
    }
    iterations++;
    // Sleep for a short time.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
  }
  EXPECT_TRUE(object_exists);

  // The promise should have evaluated without errors. See the HTML files for
  // details.
  std::string result_error =
      content::EvalJs(web_contents, "window.resultPromise").error;
  EXPECT_TRUE(result_error.empty()) << result_error;
}

}  // namespace
