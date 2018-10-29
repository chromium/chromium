// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"

class ExecuteScriptApiTest : public extensions::ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    // We need a.com to be a little bit slow to trigger a race condition.
    host_resolver()->AddRuleWithLatency("a.com", "127.0.0.1", 500);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptBasic) {
  ASSERT_TRUE(RunExtensionTest("executescript/basic")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptBadEncoding) {
  // data/extensions/api_test/../bad = data/extensions/bad
  ASSERT_TRUE(RunExtensionTest("../bad")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptInFrame) {
  ASSERT_TRUE(RunExtensionTest("executescript/in_frame")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptByFrameId) {
  ASSERT_TRUE(RunExtensionTest("executescript/frame_id")) << message_;
}

// Fails often on Windows.
// http://crbug.com/174715
#if defined(OS_WIN)
#define MAYBE_ExecuteScriptPermissions DISABLED_ExecuteScriptPermissions
#else
#define MAYBE_ExecuteScriptPermissions ExecuteScriptPermissions
#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, MAYBE_ExecuteScriptPermissions) {
  ASSERT_TRUE(RunExtensionTest("executescript/permissions")) << message_;
}

// If failing, mark disabled and update http://crbug.com/84760.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptFileAfterClose) {
  ASSERT_TRUE(RunExtensionTest("executescript/file_after_close")) << message_;
}

// If crashing, mark disabled and update http://crbug.com/67774.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptFragmentNavigation) {
  const char extension_name[] = "executescript/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_NavigationRaceExecuteScript DISABLED_NavigationRaceExecuteScript
#else
#define MAYBE_NavigationRaceExecuteScript NavigationRaceExecuteScript
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest,
                       MAYBE_NavigationRaceExecuteScript) {
  ASSERT_TRUE(RunExtensionSubtest("executescript/navigation_race",
                                  "execute_script.html")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptFrameAfterLoad) {
  ASSERT_TRUE(RunExtensionTest("executescript/frame_after_load")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, FrameWithHttp204) {
  ASSERT_TRUE(RunExtensionTest("executescript/http204")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptRunAt) {
  ASSERT_TRUE(RunExtensionTest("executescript/run_at")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptCSSOrigin) {
  ASSERT_TRUE(RunExtensionTest("executescript/css_origin")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptCallback) {
  ASSERT_TRUE(RunExtensionTest("executescript/callback")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, UserGesture) {
  ASSERT_TRUE(RunExtensionTest("executescript/user_gesture")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, InjectIntoSubframesOnLoad) {
  ASSERT_TRUE(RunExtensionTest("executescript/subframes_on_load")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, RemovedFrames) {
  ASSERT_TRUE(RunExtensionTest("executescript/removed_frames")) << message_;
}

// Ensure that an extension can inject a script in a file frame provided it has
// access to file urls enabled and the necessary host permissions.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, InjectScriptInFileFrameAllowed) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  SetCustomArg("ALLOWED");
  ASSERT_TRUE(RunExtensionTest("executescript/file_access")) << message_;
}

// Ensure that an extension can't inject a script in a file frame if it doesn't
// have file access.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, InjectScriptInFileFrameDenied) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  SetCustomArg("DENIED");
  ASSERT_TRUE(RunExtensionTestNoFileAccess("executescript/file_access"))
      << message_;
}

// If tests time out because it takes too long to run them, then this value can
// be increased to split the DestructiveScriptTest tests in approximately equal
// parts. Each part takes approximately the same time to run.
const int kDestructiveScriptTestBucketCount = 1;

class DestructiveScriptTest : public ExecuteScriptApiTest,
                              public testing::WithParamInterface<int> {
 protected:
  // The test extension selects the sub test based on the host name.
  bool RunSubtest(const std::string& test_host) {
    return RunExtensionSubtest(
        "executescript/destructive",
        "test.html?" + test_host +
        "#bucketcount=" + base::IntToString(kDestructiveScriptTestBucketCount) +
        "&bucketindex=" + base::IntToString(GetParam()));
  }
};

// Removes the frame as soon as the content script is executed.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, SynchronousRemoval) {
  ASSERT_TRUE(RunSubtest("synchronous")) << message_;
}

// Removes the frame at the frame's first scheduled microtask.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, MicrotaskRemoval) {
  ASSERT_TRUE(RunSubtest("microtask")) << message_;
}

// Removes the frame at the frame's first scheduled macrotask.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, MacrotaskRemoval) {
  ASSERT_TRUE(RunSubtest("macrotask")) << message_;
}

// Removes the frame at the first DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMNodeInserted1) {
  ASSERT_TRUE(RunSubtest("domnodeinserted1")) << message_;
}

// Removes the frame at the second DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMNodeInserted2) {
  ASSERT_TRUE(RunSubtest("domnodeinserted2")) << message_;
}

// Removes the frame at the third DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMNodeInserted3) {
  ASSERT_TRUE(RunSubtest("domnodeinserted3")) << message_;
}

// Removes the frame at the first DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMSubtreeModified1) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified1")) << message_;
}

// Removes the frame at the second DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMSubtreeModified2) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified2")) << message_;
}

// Removes the frame at the third DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DOMSubtreeModified3) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified3")) << message_;
}

INSTANTIATE_TEST_CASE_P(ExecuteScriptApiTest,
                        DestructiveScriptTest,
                        ::testing::Range(0, kDestructiveScriptTestBucketCount));
