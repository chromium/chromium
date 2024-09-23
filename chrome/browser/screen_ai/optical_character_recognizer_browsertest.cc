// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
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

void WaitForDisconnecting(screen_ai::ScreenAIServiceRouter* router,
                          base::OnceCallback<void()> callback,
                          int remaining_tries) {
  if (!router->IsProcessRunningForTesting() || !remaining_tries) {
    std::move(callback).Run();
    return;
  }
  router->ShutDownIfNoClientsForTesting();

  // Wait more...
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitForDisconnecting, router, std::move(callback),
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

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)

// Computes string match based on the edit distance of the two strings.
// Returns 0 for totally different strings and 1 for totally matching. See
// `StringMatchTest` for some examples.
double StringMatch(std::string_view expected, std::string_view extracted) {
  unsigned extracted_size = extracted.size();
  unsigned expected_size = expected.size();
  if (!expected_size || !extracted_size) {
    return (expected_size || extracted_size) ? 0 : 1;
  }

  // d[i][j] is the best match (shortest edit distance) until expected[i] and
  // extracted[j].
  std::vector<std::vector<int>> d(expected_size,
                                  std::vector<int>(extracted_size));

  for (unsigned i = 0; i < expected_size; i++) {
    for (unsigned j = 0; j < extracted_size; j++) {
      int local_match = (expected[i] == extracted[j]) ? 0 : 1;
      if (!i && !j) {
        d[i][j] = local_match;
        continue;
      }

      int best_match = expected_size + extracted_size;
      // Insert
      if (i) {
        best_match = std::min(best_match, d[i - 1][j] + 1);
      }
      // Delete
      if (j) {
        best_match = std::min(best_match, d[i][j - 1] + 1);
      }
      // Replace/Accept
      if (i && j) {
        best_match = std::min(best_match, d[i - 1][j - 1] + local_match);
      }
      d[i][j] = best_match;
    }
  }
  return 1.0f -
         (d[expected_size - 1][extracted_size - 1]) /
             static_cast<double>(std::max(expected_size, extracted_size));
}

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) &&
        // !BUILDFLAG(USE_FAKE_SCREEN_AI)

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
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
      ScreenAIInstallState::GetInstance()->SetComponentFolder(
          GetComponentBinaryPathForTests().DirName());
#else
      NOTREACHED() << "Test library is used on a not-suppported platform.";
#endif
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

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_Empty) {
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

// The image used in this test is very simple to reduce the possibility of
// failure due to library changes.
// If this test fails after updating the library, there is a high probability
// that the new library has some sort of incompatibility with Chromium.
IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_Simple) {
  base::HistogramTester histograms;

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
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

// Fake library always returns empty.
#if BUILDFLAG(USE_FAKE_SCREEN_AI)
  bool expected_call_success = false;
#else
  bool expected_call_success = true;
#endif

  auto& results = perform_future.Get<mojom::VisualAnnotationPtr>();
  unsigned expected_lines_count =
      (expected_call_success && IsOcrAvailable()) ? 1 : 0;
  ASSERT_EQ(expected_lines_count, results->lines.size());
  if (results->lines.size()) {
    ASSERT_EQ(results->lines[0]->text_line, "A");
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  unsigned expected_calls = IsOcrAvailable() ? 1 : 0;
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.ClientType",
                              expected_calls);
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.ClientType",
                               mojom::OcrClientType::kTest, expected_calls);

  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Successful",
                              expected_calls);
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.Successful",
                               expected_call_success, expected_calls);

  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.LinesCount",
                              expected_calls);
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.LinesCount",
                               expected_lines_count, expected_calls);

  int image_size = bitmap.width() * bitmap.height();
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.ImageSize10M",
                              expected_calls);
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.ImageSize10M",
                               image_size, expected_calls);

  // Expect measured latency, but we don't know how long it taskes to process.
  // So we just check the total count of the expected bucket determined by the
  // image size.
  EXPECT_GT(500 * 500, image_size);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Latency.Small",
                              expected_calls);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Latency.Medium", 0);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Latency.Large", 0);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Latency.XLarge", 0);
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       PerformOCR_AfterServiceRevive) {
  if (!IsOcrAvailable()) {
    GTEST_SKIP() << "This test is only available when service is available";
  }

  screen_ai::ScreenAIServiceRouter* router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile());

  // Init OCR once and verify service availability.
  {
    base::test::TestFuture<bool> init_future;
    scoped_refptr<OpticalCharacterRecognizer> ocr =
        OpticalCharacterRecognizer::CreateWithStatusCallback(
            browser()->profile(), mojom::OcrClientType::kTest,
            init_future.GetCallback());
    ASSERT_TRUE(init_future.Wait());
    ASSERT_TRUE(init_future.Get<bool>());

    ASSERT_TRUE(router->IsProcessRunningForTesting());
  }

  // Trigger service shut down and wait for disconnecting.
  base::test::TestFuture<void> future;
  WaitForDisconnecting(router, future.GetCallback(), /*remaining_tries=*/10);
  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(router->IsProcessRunningForTesting());

  // Init OCR again.
  base::test::TestFuture<bool> init_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());
  ASSERT_TRUE(init_future.Get<bool>());
  ASSERT_TRUE(router->IsProcessRunningForTesting());

  // Perform OCR.
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

  // Fake library always returns empty results.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  ASSERT_FALSE(perform_future.Get<mojom::VisualAnnotationPtr>()->lines.empty());
#endif
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       PerformOCR_AfterDisconnect) {
  if (!IsOcrAvailable()) {
    GTEST_SKIP() << "This test is only available when service is available";
  }

  // Init OCR.
  base::test::TestFuture<bool> init_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());
  ASSERT_TRUE(init_future.Get<bool>());

  ocr->DisconnectForTesting();

  // Perform OCR and get VisualAnnotation.
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

  // Fake library always returns empty results.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  ASSERT_FALSE(perform_future.Get<mojom::VisualAnnotationPtr>()->lines.empty());
#endif

  ocr->DisconnectForTesting();

  // Perform OCR and get AxTreeUpdate.
  base::test::TestFuture<const ui::AXTreeUpdate&> perform_future2;
  ocr->PerformOCR(bitmap, perform_future2.GetCallback());
  ASSERT_TRUE(perform_future2.Wait());

  // Fake library always returns empty results.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  ASSERT_FALSE(perform_future2.Get<ui::AXTreeUpdate>().nodes.empty());
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpticalCharacterRecognizerTest,
                         ::testing::Combine(testing::Bool(), testing::Bool()),
                         OpticalCharacterRecognizerTestParamsToString());

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)

TEST(OpticalCharacterRecognizer, StringMatchTest) {
  ASSERT_EQ(StringMatch("ABC", ""), 0);
  ASSERT_EQ(StringMatch("", "ABC"), 0);
  ASSERT_EQ(StringMatch("ABC", "ABC"), 1);
  ASSERT_EQ(StringMatch("ABC", "DEF"), 0);
  ASSERT_LE(StringMatch("ABCD", "ABC"), 0.75);
  ASSERT_LE(StringMatch("ABCD", "ABXD"), 0.75);
}

// Param: Test name.
class OpticalCharacterRecognizerResultsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  OpticalCharacterRecognizerResultsTest() {
    feature_list_.InitWithFeatures({ax::mojom::features::kScreenAIOCREnabled,
                                    ::features::kScreenAITestMode},
                                   {});
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        GetComponentBinaryPathForTests().DirName());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If this test fails after updating the library, the failure can be related to
// minor changes in recognition results of the library. The failed cases can be
// checked and if the new result is acceptable. For each test case, the last
// line in the .txt file is the minimum acceptable match and it can be updated.
IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerResultsTest, PerformOCR) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Init OCR.
  base::test::TestFuture<bool> init_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());
  ASSERT_EQ(init_future.Get<bool>(), true);

  // Perform OCR.
  base::FilePath image_path =
      base::FilePath(FILE_PATH_LITERAL("ocr"))
          .AppendASCII(base::StringPrintf("%s.png", GetParam()));
  SkBitmap bitmap = LoadImageFromTestFile(image_path);
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());
  auto& results = perform_future.Get<mojom::VisualAnnotationPtr>();

  // Load expectations.
  std::string expetations;
  base::FilePath expectation_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &expectation_path));
  expectation_path =
      expectation_path.Append(FILE_PATH_LITERAL("chrome/test/data/ocr"))
          .AppendASCII(base::StringPrintf("%s.txt", GetParam()));
  ASSERT_TRUE(ReadFileToString(expectation_path, &expetations))
      << expectation_path << " not found.";
  std::vector<std::string> expected_lines = base::SplitString(
      expetations, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // The last line of expectations is the minimum expected match.
  ASSERT_NE(expected_lines.size(), 0u);
  double expected_match =
      atof(expected_lines[expected_lines.size() - 1].c_str());
  expected_lines.pop_back();

  ASSERT_EQ(results->lines.size(), expected_lines.size());
  for (unsigned i = 0; i < results->lines.size(); i++) {
    const std::string extracted_text = results->lines[i]->text_line;
    const std::string expected_text = expected_lines[i];

    double match = StringMatch(expected_text, extracted_text);
    ASSERT_GE(match, expected_match) << "Extracted text: " << extracted_text;
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpticalCharacterRecognizerResultsTest,
                         testing::Values("simple_text_only_sample",
                                         "chinese",
                                         "farsi",
                                         "hindi",
                                         "japanese",
                                         "korean",
                                         "russian"));

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !
        // BUILDFLAG(USE_FAKE_SCREEN_AI)
}  // namespace screen_ai
