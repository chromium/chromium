// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

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

void WaitForStatus(scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
                   base::OnceCallback<void(void)> callback,
                   int remaining_tries) {
  if (ocr->StatusAvailableForTesting() || !remaining_tries) {
    std::move(callback).Run();
    return;
  }

  // Wait more...
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitForStatus, ocr, std::move(callback),
                     remaining_tries - 1),
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
      screen_ai::OpticalCharacterRecognizer::Create(
          browser()->profile(), mojom::OcrClientType::kTest);
  base::test::TestFuture<void> future;
  // This step can be slow.
  WaitForStatus(ocr, future.GetCallback(), /*remaining_tries=*/25);
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(ocr->StatusAvailableForTesting());
  EXPECT_EQ(ocr->is_ready(), IsOcrAvailable());
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       CreateWithStatusCallback) {
  base::test::TestFuture<bool> future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(future.Get<bool>(), IsOcrAvailable());
  ASSERT_TRUE(ocr);
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR) {
  // Init OCR.
  base::test::TestFuture<bool> init_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());
  ASSERT_EQ(init_future.Get<bool>(), IsOcrAvailable());

  // Perform OCR.
  SkBitmap bitmap =
      LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL("ocr/empty.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());
  ASSERT_TRUE(perform_future.Get<mojom::VisualAnnotationPtr>()->lines.empty());
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_WithResults) {
  // Init OCR.
  base::test::TestFuture<bool> init_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());
  ASSERT_EQ(init_future.Get<bool>(), IsOcrAvailable());

  // Perform OCR.
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/quick_brown_fox.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

// Fake library always returns empty.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  auto& results = perform_future.Get<mojom::VisualAnnotationPtr>();
  ASSERT_EQ(results->lines.size(), IsOcrAvailable() ? 6u : 0u);
  if (results->lines.size()) {
    ASSERT_EQ(results->lines[0]->text_line,
              "The quick brown fox jumps over the lazy dog.");
  }
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpticalCharacterRecognizerTest,
                         ::testing::Combine(testing::Bool(), testing::Bool()),
                         OpticalCharacterRecognizerTestParamsToString());

}  // namespace screen_ai
