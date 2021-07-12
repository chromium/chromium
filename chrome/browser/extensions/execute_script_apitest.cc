// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

class ExecuteScriptApiTestBase : public ExtensionApiTest {
 public:
  ExecuteScriptApiTestBase() = default;
  ~ExecuteScriptApiTestBase() override = default;
  ExecuteScriptApiTestBase(const ExecuteScriptApiTestBase&) = delete;
  ExecuteScriptApiTestBase& operator=(const ExecuteScriptApiTestBase&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // We need a.com to be a little bit slow to trigger a race condition.
    host_resolver()->AddRuleWithLatency("a.com", "127.0.0.1", 500);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

class ExecuteScriptApiTest : public ExecuteScriptApiTestBase,
                             public testing::WithParamInterface<ContextType> {
 public:
  ExecuteScriptApiTest() = default;
  ~ExecuteScriptApiTest() override = default;
  ExecuteScriptApiTest(const ExecuteScriptApiTest&) = delete;
  ExecuteScriptApiTest& operator=(const ExecuteScriptApiTest&) = delete;

 protected:
  bool RunTest(const char* extension_name, bool allow_file_access = false) {
    return RunExtensionTest(
        extension_name, {},
        {.allow_file_access = allow_file_access,
         .load_as_service_worker = GetParam() == ContextType::kServiceWorker});
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExecuteScriptApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExecuteScriptApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptBasic) {
  ASSERT_TRUE(RunTest("executescript/basic")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptBadEncoding) {
  ASSERT_TRUE(RunTest("executescript/bad_encoding")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptInFrame) {
  ASSERT_TRUE(RunTest("executescript/in_frame")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptByFrameId) {
  ASSERT_TRUE(RunTest("executescript/frame_id")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptPermissions) {
  ASSERT_TRUE(RunTest("executescript/permissions")) << message_;
}

// If failing, mark disabled and update http://crbug.com/84760.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFileAfterClose) {
  ASSERT_TRUE(RunTest("executescript/file_after_close")) << message_;
}

// If crashing, mark disabled and update http://crbug.com/67774.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFragmentNavigation) {
  ASSERT_TRUE(RunTest("executescript/fragment")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, NavigationRaceExecuteScript) {
  ASSERT_TRUE(RunTest("executescript/navigation_race")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFrameAfterLoad) {
  ASSERT_TRUE(RunTest("executescript/frame_after_load")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, FrameWithHttp204) {
  ASSERT_TRUE(RunTest("executescript/http204")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptRunAt) {
  ASSERT_TRUE(RunTest("executescript/run_at")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptCSSOrigin) {
  ASSERT_TRUE(RunTest("executescript/css_origin")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptCallback) {
  ASSERT_TRUE(RunTest("executescript/callback")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptRemoveCSS) {
  ASSERT_TRUE(RunTest("executescript/remove_css")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, UserGesture) {
  // TODO(https://crbug.com/977629): Gesture support for testing is not
  // available for Service Worker-based extensions.
  if (GetParam() == ContextType::kServiceWorker)
    return;

  ASSERT_TRUE(RunTest("executescript/user_gesture")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectIntoSubframesOnLoad) {
  ASSERT_TRUE(RunTest("executescript/subframes_on_load")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, RemovedFrames) {
  ASSERT_TRUE(RunTest("executescript/removed_frames")) << message_;
}

// Ensure that an extension can inject a script in a file frame provided it has
// access to file urls enabled and the necessary host permissions.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectScriptInFileFrameAllowed) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  SetCustomArg("ALLOWED");
  ASSERT_TRUE(RunTest("executescript/file_access",
                      /*allow_file_access=*/true))
      << message_;
}

// Ensure that an extension can't inject a script in a file frame if it doesn't
// have file access.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectScriptInFileFrameDenied) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  SetCustomArg("DENIED");
  ASSERT_TRUE(RunTest("executescript/file_access")) << message_;
}

// If tests time out because it takes too long to run them, then this value can
// be increased to split the DestructiveScriptTest tests in approximately equal
// parts. Each part takes approximately the same time to run.
const int kDestructiveScriptTestBucketCount = 1;

class DestructiveScriptTest : public ExecuteScriptApiTestBase,
                              public testing::WithParamInterface<int> {
 protected:
  // The test extension selects the sub test based on the host name.
  bool RunSubtest(const std::string& test_host) {
    const std::string page_url =
        "test.html?" + test_host + "#bucketcount=" +
        base::NumberToString(kDestructiveScriptTestBucketCount) +
        "&bucketindex=" + base::NumberToString(GetParam());
    return RunExtensionTest("executescript/destructive",
                            {.page_url = page_url.c_str()});
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

// TODO(http://crbug.com/1028308): Flaky on multiple platforms
// Removes the frame at the frame's first scheduled macrotask.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTest, DISABLED_MacrotaskRemoval) {
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

INSTANTIATE_TEST_SUITE_P(ExecuteScriptApiTest,
                         DestructiveScriptTest,
                         ::testing::Range(0,
                                          kDestructiveScriptTestBucketCount));

}  // namespace extensions
