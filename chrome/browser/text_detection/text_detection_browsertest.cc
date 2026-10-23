// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace text_detection {

namespace {

class TextDetectionBrowserTest
    : public InProcessBrowserTest,
      public screen_ai::ScreenAIInstallState::Observer,
      public ::testing::WithParamInterface<bool> {
 public:
  TextDetectionBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        ax::mojom::features::kScreenAIOCREnabled,
        blink::features::kTextDetector,
    };
    if (IsOcrLibraryAvailable()) {
      enabled_features.push_back(features::kScreenAITestMode);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  ~TextDetectionBrowserTest() override = default;

  bool IsOcrLibraryAvailable() const {
    return GetParam() && screen_ai::ScreenAIInstallState::IsDeviceCompatible();
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    if (IsOcrLibraryAvailable()) {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
#else
      NOTREACHED();
#endif
    } else {
      component_download_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }

  void TearDownOnMainThread() override {
    component_download_observer_.Reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    // `component_download_observer_` is only registered in
    // `SetUpOnMainThread()` when `!IsOcrLibraryAvailable()`. In that
    // configuration, Component Updater cannot download the real ScreenAI
    // library in browser tests and would stay in `kDownloading` indefinitely,
    // so transition to `kDownloadFailed` to allow OCR initialization
    // (`EnsureReady()`) to complete with failure.
    CHECK(!IsOcrLibraryAvailable());
    if (state == screen_ai::ScreenAIInstallState::State::kDownloading) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            screen_ai::ScreenAIInstallState::GetInstance()->SetState(
                screen_ai::ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_download_observer_{this};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TextDetectionBrowserTest, CreateAndDetect) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  if (IsOcrLibraryAvailable()) {
    constexpr char kDetectScript[] = R"(
      (async () => {
        const detector = await TextDetector.create();
        const img = new Image();
        img.src = '/ocr/simple_text_only_sample.png';
        await img.decode();
        const results = await detector.detect(img);
        return results.map(r => r.rawValue).join('\n');
      })()
    )";
    EXPECT_EQ("ABC\nDEF\nGHI\nJKL",
              content::EvalJs(web_contents, kDetectScript));
  } else {
    constexpr char kCreateRejectsScript[] = R"(
      (async () => {
        try {
          await TextDetector.create();
          return 'RESOLVED';
        } catch (e) {
          return e.name;
        }
      })()
    )";
    EXPECT_EQ("NotSupportedError",
              content::EvalJs(web_contents, kCreateRejectsScript));
  }
}

// Parameterizes tests by whether the real ScreenAI OCR library is available
// (`true` = `OcrAvailable`, `false` = `OcrUnavailable`).
// `ENABLE_SCREEN_AI_BROWSERTESTS` indicates that the prebuilt ScreenAI binary
// is downloaded via CIPD on this platform, and `!USE_FAKE_SCREEN_AI` excludes
// sanitizer builds (MSan/UBSan) that link a no-op fake library returning empty
// OCR results. When the real library cannot be used, only `OcrUnavailable` is
// instantiated to test the `NotSupportedError` rejection path.
INSTANTIATE_TEST_SUITE_P(All,
                         TextDetectionBrowserTest,
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)
                         ::testing::Bool(),
#else
    ::testing::Values(false),
#endif
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "OcrAvailable"
                                             : "OcrUnavailable";
                         });

}  // namespace

}  // namespace text_detection
