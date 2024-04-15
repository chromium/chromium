// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cfi_buildflags.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/utils/content_script_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/common/features.h"

namespace extensions {

namespace {

struct BackForwardCacheDisabledDestructiveScriptTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<std::tuple<int, bool>>& i) const {
    return base::StringPrintf("%s_BUCKET_%d",
                              std::get<1>(i.param) ? "OOPIF" : "GUESTVIEW",
                              std::get<0>(i.param));
  }
};

}  // namespace

using ContextType = ExtensionApiTest::ContextType;

class ExecuteScriptApiTestBase : public ExtensionApiTest {
 public:
  explicit ExecuteScriptApiTestBase(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
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
  ExecuteScriptApiTest() : ExecuteScriptApiTestBase(GetParam()) {}
  ~ExecuteScriptApiTest() override = default;
  ExecuteScriptApiTest(const ExecuteScriptApiTest&) = delete;
  ExecuteScriptApiTest& operator=(const ExecuteScriptApiTest&) = delete;

 protected:
  bool RunTest(const char* extension_name, bool allow_file_access = false) {
    return RunExtensionTest(extension_name, {},
                            {.allow_file_access = allow_file_access});
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExecuteScriptApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
// These tests use chrome.tabs.executeScript, which is not available in MV3.
// See crbug.com/332328868.
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExecuteScriptApiTest,
                         ::testing::Values(ContextType::kServiceWorkerMV2));

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptBasic) {
  ASSERT_TRUE(RunExtensionTest("executescript/basic")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptBadEncoding) {
  ASSERT_TRUE(RunExtensionTest("executescript/bad_encoding")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptInFrame) {
  ASSERT_TRUE(RunExtensionTest("executescript/in_frame")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptByFrameId) {
  ASSERT_TRUE(RunExtensionTest("executescript/frame_id")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptPermissions) {
  ASSERT_TRUE(RunExtensionTest("executescript/permissions")) << message_;
}

// If failing, mark disabled and update http://crbug.com/84760.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFileAfterClose) {
  ASSERT_TRUE(RunExtensionTest("executescript/file_after_close")) << message_;
}

// If crashing, mark disabled and update http://crbug.com/67774.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFragmentNavigation) {
  ASSERT_TRUE(RunExtensionTest("executescript/fragment")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, NavigationRaceExecuteScript) {
  ASSERT_TRUE(RunExtensionTest("executescript/navigation_race")) << message_;
}

// If failing, mark disabled and update http://crbug.com/92105.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptFrameAfterLoad) {
  ASSERT_TRUE(RunExtensionTest("executescript/frame_after_load")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, FrameWithHttp204) {
  ASSERT_TRUE(RunExtensionTest("executescript/http204")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptRunAt) {
  ASSERT_TRUE(RunExtensionTest("executescript/run_at")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptCSSOrigin) {
  ASSERT_TRUE(RunExtensionTest("executescript/css_origin")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptCallback) {
  ASSERT_TRUE(RunExtensionTest("executescript/callback")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptRemoveCSS) {
  ASSERT_TRUE(RunTest("executescript/remove_css")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, UserGesture) {
  ASSERT_TRUE(RunExtensionTest("executescript/user_gesture")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectIntoSubframesOnLoad) {
  ASSERT_TRUE(RunExtensionTest("executescript/subframes_on_load")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, RemovedFrames) {
  ASSERT_TRUE(RunExtensionTest("executescript/removed_frames")) << message_;
}

// Tests that tabs.executeScript called with files exceeding the max size limit
// will return an error and not execute.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, ExecuteScriptSizeLimit) {
  auto single_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptLengthForTesting(700u);
  ASSERT_TRUE(RunExtensionTest("executescript/script_size_limit")) << message_;
}

// Ensure that an extension can inject a script in a file frame provided it has
// access to file urls enabled and the necessary host permissions.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectScriptInFileFrameAllowed) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           net::FilePathToFileURL(test_file)));

  SetCustomArg("ALLOWED");
  ASSERT_TRUE(RunExtensionTest("executescript/file_access", {},
                               {.allow_file_access = true}))
      << message_;
}

// Ensure that an extension can't inject a script in a file frame if it doesn't
// have file access.
IN_PROC_BROWSER_TEST_P(ExecuteScriptApiTest, InjectScriptInFileFrameDenied) {
  // Navigate to a file url. The extension will subsequently try to inject a
  // script into it.
  base::FilePath test_file =
      test_data_dir_.DirName().AppendASCII("test_file.txt");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           net::FilePathToFileURL(test_file)));

  SetCustomArg("DENIED");
  ASSERT_TRUE(RunExtensionTest("executescript/file_access")) << message_;
}

// If tests time out because it takes too long to run them, then this value can
// be increased to split the DestructiveScriptTest tests in approximately equal
// parts. Each part takes approximately the same time to run.
const int kDestructiveScriptTestBucketCount = 1;

class DestructiveScriptTest : public ExecuteScriptApiTestBase {
 public:
  virtual int GetBucketIndex() const = 0;

 protected:
  // The test extension selects the sub test based on the host name.
  bool RunSubtest(const std::string& test_host) {
    const std::string extension_url =
        "test.html?" + test_host + "#bucketcount=" +
        base::NumberToString(kDestructiveScriptTestBucketCount) +
        "&bucketindex=" + base::NumberToString(GetBucketIndex());
    return RunExtensionTest("executescript/destructive",
                            {.extension_url = extension_url.c_str()});
  }
};

// For destructive script tests that don't involve PDFs and therefore don't need
// to run in OOPIF PDF viewer mode.
class DestructiveScriptTestWithoutOopifOverride
    : public DestructiveScriptTest,
      public testing::WithParamInterface<int> {
 public:
  int GetBucketIndex() const override { return GetParam(); }
};

// For destructive script tests that require BFCache to be disabled and involve
// PDFs.
class BackForwardCacheDisabledDestructiveScriptTest
    : public DestructiveScriptTest,
      public testing::WithParamInterface<std::tuple<int, bool>> {
 public:
  int GetBucketIndex() const override { return std::get<0>(GetParam()); }

  bool UseOopif() const { return std::get<1>(GetParam()); }

 private:
  void SetUp() override {
    // The SynchronousRemoval and MicrotaskRemoval tests seem to be especially
    // flaky when same-site back/forward cache is enabled, so disable the
    // feature.
    // TODO(crbug.com/40820215): Fix the flakiness.
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features{
        features::kBackForwardCache};
    if (UseOopif()) {
      enabled_features.push_back(chrome_pdf::features::kPdfOopif);
    } else {
      disabled_features.push_back(chrome_pdf::features::kPdfOopif);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    DestructiveScriptTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on ASAN and -dbg, and Linux CFI bots. crbug.com/1293865
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG) || \
    (BUILDFLAG(CFI_ICALL_CHECK) && BUILDFLAG(IS_LINUX))
#define MAYBE_SynchronousRemoval DISABLED_SynchronousRemoval
#else
#define MAYBE_SynchronousRemoval SynchronousRemoval
#endif
// Removes the frame as soon as the content script is executed.
IN_PROC_BROWSER_TEST_P(BackForwardCacheDisabledDestructiveScriptTest,
                       MAYBE_SynchronousRemoval) {
  ASSERT_TRUE(RunSubtest("synchronous")) << message_;
}

// Flaky on ASAN and -dbg and Linux CFI. crbug.com/1293865
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG) || \
    (BUILDFLAG(CFI_ICALL_CHECK) && BUILDFLAG(IS_LINUX))
#define MAYBE_MicrotaskRemoval DISABLED_MicrotaskRemoval
#else
#define MAYBE_MicrotaskRemoval MicrotaskRemoval
#endif
// Removes the frame at the frame's first scheduled microtask.
IN_PROC_BROWSER_TEST_P(BackForwardCacheDisabledDestructiveScriptTest,
                       MAYBE_MicrotaskRemoval) {
  ASSERT_TRUE(RunSubtest("microtask")) << message_;
}

// TODO(http://crbug.com/1028308): Flaky on multiple platforms
// Removes the frame at the frame's first scheduled macrotask.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DISABLED_MacrotaskRemoval) {
  ASSERT_TRUE(RunSubtest("macrotask")) << message_;
}

// Removes the frame at the first DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMNodeInserted1) {
  ASSERT_TRUE(RunSubtest("domnodeinserted1")) << message_;
}

// Removes the frame at the second DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMNodeInserted2) {
  ASSERT_TRUE(RunSubtest("domnodeinserted2")) << message_;
}

// Removes the frame at the third DOMNodeInserted event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMNodeInserted3) {
  ASSERT_TRUE(RunSubtest("domnodeinserted3")) << message_;
}

// Removes the frame at the first DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMSubtreeModified1) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified1")) << message_;
}

// Removes the frame at the second DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMSubtreeModified2) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified2")) << message_;
}

// Removes the frame at the third DOMSubtreeModified event.
IN_PROC_BROWSER_TEST_P(DestructiveScriptTestWithoutOopifOverride,
                       DOMSubtreeModified3) {
  ASSERT_TRUE(RunSubtest("domsubtreemodified3")) << message_;
}

INSTANTIATE_TEST_SUITE_P(ExecuteScriptApiTest,
                         DestructiveScriptTestWithoutOopifOverride,
                         ::testing::Range(0,
                                          kDestructiveScriptTestBucketCount));

// TODO(crbug.com/40268279): Stop testing GuestView PDF viewer once OOPIF PDF
// viewer launches.
INSTANTIATE_TEST_SUITE_P(
    ExecuteScriptApiTest,
    BackForwardCacheDisabledDestructiveScriptTest,
    testing::Combine(::testing::Range(0, kDestructiveScriptTestBucketCount),
                     testing::Bool()),
    BackForwardCacheDisabledDestructiveScriptTestPassToString());

class ExecuteScriptApiFencedFrameTest : public ExecuteScriptApiTestBase {
 protected:
  ExecuteScriptApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{blink::features::kFencedFrames, {}},
                              {blink::features::kFencedFramesAPIChanges, {}},
                              {blink::features::kFencedFramesDefaultMode, {}},
                              {features::kPrivacySandboxAdsAPIsOverride, {}}},
        /*disabled_features=*/{features::kSpareRendererForSitePerProcess});
    // Fenced frames are only allowed in secure contexts.
    UseHttpsTestServer();
  }
  ~ExecuteScriptApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiFencedFrameTest, Load) {
  ASSERT_TRUE(RunExtensionTest("executescript/fenced_frames")) << message_;
}

}  // namespace extensions
