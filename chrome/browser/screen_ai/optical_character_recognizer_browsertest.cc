// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

// TODO(crbug.com/41489544): Update when fake ScreenAI library accepts
// `PerformOCR` requests.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
#define ENABLE_PERFORM_OCR_TESTS

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#endif

namespace {

#if defined(ENABLE_PERFORM_OCR_TESTS)
// Load image from test data directory.
SkBitmap LoadImageFromTestFile(
    const base::FilePath& relative_path_from_chrome_data) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath chrome_src_dir;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &chrome_src_dir));

  base::FilePath image_path =
      chrome_src_dir.Append(FILE_PATH_LITERAL("chrome/test/data"))
          .Append(relative_path_from_chrome_data);
  EXPECT_TRUE(base::PathExists(image_path));

  std::string image_data;
  EXPECT_TRUE(base::ReadFileToString(image_path, &image_data));

  SkBitmap image;
  EXPECT_TRUE(
      gfx::PNGCodec::Decode(reinterpret_cast<const uint8_t*>(image_data.data()),
                            image_data.size(), &image));
  return image;
}
#endif  // defined(ENABLE_PERFORM_OCR_TESTS)

void WaitForStatus(scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
                   base::RunLoop* run_loop,
                   int remaining_tries) {
  if (ocr->StatusAvailableForTesting() || !remaining_tries) {
    run_loop->Quit();
    return;
  }

  // Wait more...
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitForStatus, ocr, run_loop, remaining_tries - 1),
      base::Milliseconds(200));
}

// bool: PDF OCR service enabled.
// bool: ScreenAI library available.
using OpticalCharacterRecognizerTestParams = std::tuple<bool, bool>;

struct OpticalCharacterRecognizerTestParamsToString {
  std::string operator()(
      const ::testing::TestParamInfo<OpticalCharacterRecognizerTestParams>&
          info) const {
    return base::StringPrintf(
        "OCR_%s_Library_%s", std::get<0>(info.param) ? "Enabled" : "Disabled",
        std::get<1>(info.param) ? "Available" : "Unavailable");
  }
};

}  // namespace
namespace screen_ai {

class OpticalCharacterRecognizerTest
    : public InProcessBrowserTest,
      public ScreenAIInstallState::Observer,
      public ::testing::WithParamInterface<
          OpticalCharacterRecognizerTestParams> {
 public:
  OpticalCharacterRecognizerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsOcrServiceEnabled()) {
      enabled_features.push_back(ax::mojom::features::kScreenAIOCREnabled);
    } else {
      disabled_features.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }

    if (IsLibraryAvailable()) {
      enabled_features.push_back(::features::kScreenAITestMode);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~OpticalCharacterRecognizerTest() override = default;

  bool IsOcrServiceEnabled() const { return std::get<0>(GetParam()); }
  bool IsLibraryAvailable() const {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    return std::get<1>(GetParam());
#else
    return false;
#endif
  }

  bool IsOcrAvailable() const {
    return IsOcrServiceEnabled() && IsLibraryAvailable();
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    if (IsLibraryAvailable()) {
      ScreenAIInstallState::GetInstance()->SetComponentFolder(
          GetComponentBinaryPathForTests().DirName());
    } else {
      // Set an observer to reply download failed, when download requested.
      component_download_observer_.Observe(ScreenAIInstallState::GetInstance());
    }
  }

  // InProcessBrowserTest:
  void TearDownOnMainThread() override {
    // The Observer should be removed before browser shut down and destruction
    // of the ScreenAIInstallState.
    component_download_observer_.Reset();
  }

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override {
    if (state == ScreenAIInstallState::State::kDownloading &&
        !IsLibraryAvailable()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            ScreenAIInstallState::GetInstance()->SetState(
                ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

 private:
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_download_observer_{this};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, Create) {
  scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr =
      screen_ai::OpticalCharacterRecognizer::Create(browser()->profile());
  base::RunLoop run_loop;
  // This step can be slow.
  WaitForStatus(ocr, &run_loop, /*remaining_tries=*/25);
  run_loop.Run();

  EXPECT_TRUE(ocr->StatusAvailableForTesting());
  EXPECT_EQ(ocr->is_ready(), IsOcrAvailable());
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       CreateWithStatusCallback) {
  base::RunLoop run_loop;

  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), base::BindOnce(
                                    [](base::RunLoop* run_loop,
                                       bool expected_result, bool successful) {
                                      EXPECT_EQ(expected_result, successful);
                                      run_loop->Quit();
                                    },
                                    &run_loop, IsOcrAvailable()));
  run_loop.Run();
  EXPECT_TRUE(ocr);
}

#if defined(ENABLE_PERFORM_OCR_TESTS)

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR) {
  // Init OCR.
  base::RunLoop init_run_loop;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), base::BindOnce(
                                    [](base::RunLoop* run_loop,
                                       bool expected_result, bool successful) {
                                      EXPECT_EQ(expected_result, successful);
                                      run_loop->Quit();
                                    },
                                    &init_run_loop, IsOcrAvailable()));
  init_run_loop.Run();

  // Perform OCR.
  base::RunLoop perform_run_loop;
  SkBitmap bitmap =
      LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL("ocr/empty.png")));
  ocr->PerformOCR(bitmap, base::BindOnce(
                              [](base::RunLoop* run_loop,
                                 mojom::VisualAnnotationPtr results) {
                                EXPECT_TRUE(results);
                                EXPECT_EQ(results->lines.size(), 0u);
                                run_loop->Quit();
                              },
                              &perform_run_loop));
  perform_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_WithResults) {
  // Init OCR.
  base::RunLoop init_run_loop;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), base::BindOnce(
                                    [](base::RunLoop* run_loop,
                                       bool expected_result, bool successful) {
                                      EXPECT_EQ(expected_result, successful);
                                      run_loop->Quit();
                                    },
                                    &init_run_loop, IsOcrAvailable()));
  init_run_loop.Run();

  // Perform OCR.
  base::RunLoop perform_run_loop;
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/quick_brown_fox.png")));
  ocr->PerformOCR(
      bitmap, base::BindOnce(
                  [](base::RunLoop* run_loop, unsigned expected_lines_count,
                     mojom::VisualAnnotationPtr results) {
                    EXPECT_TRUE(results);
                    EXPECT_EQ(results->lines.size(), expected_lines_count);
                    if (results->lines.size()) {
                      EXPECT_EQ(results->lines[0]->text_line,
                                "The quick brown fox jumps over the lazy dog.");
                    }
                    run_loop->Quit();
                  },
                  &perform_run_loop, IsOcrAvailable() ? 6u : 0u));
  perform_run_loop.Run();
}

#endif  // defined(ENABLE_PERFORM_OCR_TESTS)

INSTANTIATE_TEST_SUITE_P(All,
                         OpticalCharacterRecognizerTest,
                         ::testing::Combine(testing::Bool(), testing::Bool()),
                         OpticalCharacterRecognizerTestParamsToString());

}  // namespace screen_ai
