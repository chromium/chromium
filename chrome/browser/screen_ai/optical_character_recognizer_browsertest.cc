// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
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
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// LINT.IfChange(kServiceIdleCheckingDelay)
constexpr base::TimeDelta kServiceIdleCheckingDelay = base::Seconds(3);
// LINT.ThenChange(//services/screen_ai/screen_ai_service_impl.cc:kIdleCheckingDelay)

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)
// LINT.IfChange(kResourceMeasurementInterval)
constexpr base::TimeDelta kResourceMeasurementInterval = base::Seconds(1);
// LINT.ThenChange(//chrome/browser/screen_ai/resource_monitor.cc:kSampleInterval)
#endif

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;

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
  EXPECT_TRUE(base::PathExists(image_path)) << image_path;

  std::optional<std::vector<uint8_t>> image_data =
      base::ReadFileToBytes(image_path);

  SkBitmap image = gfx::PNGCodec::Decode(image_data.value());
  EXPECT_FALSE(image.isNull());
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
  if (!router->IsProcessRunningForTesting(
          screen_ai::ScreenAIServiceRouter::Service::kOCR) ||
      !remaining_tries) {
    std::move(callback).Run();
    return;
  }

  // Wait more...
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitForDisconnecting, router, std::move(callback),
                     remaining_tries - 1),
      kServiceIdleCheckingDelay);
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

// Name of files which have results.
const int kTestFilenamesCount = 7;
std::string_view kTestFilenames[kTestFilenamesCount] = {
    "simple_text_only_sample",
    "chinese",
    "farsi",
    "hindi",
    "japanese",
    "korean",
    "russian"};

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

  // Returns true if ocr initialization was successful.
  bool CreateAndInitOCR(mojom::OcrClientType client_type) {
    base::test::TestFuture<bool> init_future;
    ocr_ = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), client_type, init_future.GetCallback());
    EXPECT_TRUE(init_future.Wait());
    return init_future.Get<bool>();
  }

  scoped_refptr<OpticalCharacterRecognizer> ocr() { return ocr_; }

 private:
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_download_observer_{this};
  scoped_refptr<OpticalCharacterRecognizer> ocr_;
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

  base::test::TestFuture<uint32_t> max_dimension_future;
  ocr->GetMaxImageDimension(max_dimension_future.GetCallback());
  ASSERT_TRUE(max_dimension_future.Wait());
  ASSERT_EQ(max_dimension_future.Get<uint32_t>(),
            IsOcrAvailable() ? screen_ai::GetMaxDimensionForOCR() : 0);
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

  base::test::TestFuture<uint32_t> max_dimension_future;
  ocr->GetMaxImageDimension(max_dimension_future.GetCallback());
  ASSERT_TRUE(max_dimension_future.Wait());
  ASSERT_EQ(max_dimension_future.Get<uint32_t>(),
            IsOcrAvailable() ? screen_ai::GetMaxDimensionForOCR() : 0);
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       CreateWithStatusCallbackWithReturnedPtrDestructed) {
  base::test::TestFuture<bool> future;

  // Create an `OpticalCharacterRecognizer` scoped_refptr and then immediately
  // discard the result.
  OpticalCharacterRecognizer::CreateWithStatusCallback(
      browser()->profile(), mojom::OcrClientType::kTest, future.GetCallback());

  // The status callback should still be run without crashing even though the
  // created scoped_refptr was destroyed.
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_Empty) {
  ASSERT_EQ(CreateAndInitOCR(mojom::OcrClientType::kTest), IsOcrAvailable());

  SkBitmap bitmap =
      LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL("ocr/empty.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());
  ASSERT_TRUE(perform_future.Get<mojom::VisualAnnotationPtr>()->lines.empty());
}

// The image used in this test is very simple to reduce the possibility of
// failure due to library changes.
// If this test fails after updating the library, there is a high probability
// that the new library has some sort of incompatibility with Chromium.
IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_Simple) {
  base::HistogramTester histograms;

  ASSERT_EQ(CreateAndInitOCR(mojom::OcrClientType::kTest), IsOcrAvailable());

  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
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

  // Expect measured latency, but we don't know how long it taskes to process.
  // So we just check the total count of the expected bucket determined by the
  // image dimensions, with threshold 2048 for each dimension.
  EXPECT_GE(2048, bitmap.width());
  EXPECT_GE(2048, bitmap.height());
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.Latency.NotDownsampled", expected_calls);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Latency.Downsampled",
                              0);
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.Downsampled.ClientType", 0);

  // PDF Specific metrics should not be recorded as the client type is test.
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.LinesCount.PDF", 0);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Time.PDF", 0);
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.ImageSize.PDF.WithText", 0);
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.ImageSize.PDF.NoText",
                              0);
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.MostDetectedLanguage.PDF", 0);
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest, PerformOCR_PdfMetrics) {
  base::HistogramTester histograms;

  ASSERT_EQ(CreateAndInitOCR(mojom::OcrClientType::kPdfViewer),
            IsOcrAvailable());

  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

// Fake library always returns empty.
#if BUILDFLAG(USE_FAKE_SCREEN_AI)
  bool expected_call_success = false;
#else
  bool expected_call_success = true;
#endif

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  unsigned expected_calls = IsOcrAvailable() ? 1 : 0;
  unsigned expected_lines_count =
      (expected_call_success && IsOcrAvailable()) ? 1 : 0;

  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.LinesCount.PDF",
                              expected_calls);
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.LinesCount.PDF",
                               expected_lines_count, expected_calls);

  // Since the text in the image is just one letter, language is not detected.
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.MostDetectedLanguage.PDF",0);

  // Expect measured latency and image size, but we don't know how long it
  // taskes to process and how large the image is.
  // So we just check the total count of the expected bucket.
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.Time.PDF",
                              expected_calls);
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.ImageSize.PDF.WithText",
      expected_lines_count);

  // If OCR is not available, the metric is not recorded at all. But when it is
  // available, the expectation is the opposite of the above metrics.
  unsigned expected_no_text_calls =
      IsOcrAvailable() ? (1 - expected_lines_count) : 0;
  histograms.ExpectTotalCount("Accessibility.ScreenAI.OCR.ImageSize.PDF.NoText",
                              expected_no_text_calls);
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       PerformOCR_ImmediatelyAfterServiceInit) {
  if (!IsOcrAvailable()) {
    GTEST_SKIP() << "This test is only available when service is available";
  }

  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_ocr_future;
  scoped_refptr<OpticalCharacterRecognizer> ocr =
      OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), mojom::OcrClientType::kTest,
          base::BindLambdaForTesting([&](bool is_successful) {
            EXPECT_TRUE(is_successful);
            // The status callback is run asynchronously after `ocr` is created
            // and assigned, so `ocr` is safe to use here.
            ASSERT_TRUE(ocr);
            ocr->PerformOCR(LoadImageFromTestFile(base::FilePath(
                                FILE_PATH_LITERAL("ocr/just_one_letter.png"))),
                            perform_ocr_future.GetCallback());
          }));

  ASSERT_TRUE(perform_ocr_future.Wait());

#if BUILDFLAG(USE_FAKE_SCREEN_AI)
  EXPECT_THAT(perform_ocr_future.Get()->lines, IsEmpty());
#else
  EXPECT_THAT(perform_ocr_future.Get()->lines,
              ElementsAre(Pointee(Field(&mojom::LineBox::text_line, "A"))));
#endif
}

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       PerformOCR_AfterServiceRevive) {
  if (!IsOcrAvailable()) {
    GTEST_SKIP() << "This test is only available when service is available";
  }

  screen_ai::ScreenAIServiceRouter* router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile());

  // Init OCR once and verify service availability.
  ASSERT_TRUE(CreateAndInitOCR(mojom::OcrClientType::kTest));
  ASSERT_TRUE(router->IsProcessRunningForTesting(
      screen_ai::ScreenAIServiceRouter::Service::kOCR));

  // Release it and wait for shutdown due to being idle.
  ocr().reset();
  base::test::TestFuture<void> future;
  WaitForDisconnecting(router, future.GetCallback(), /*remaining_tries=*/3);
  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(router->IsProcessRunningForTesting(
      screen_ai::ScreenAIServiceRouter::Service::kOCR));

  // Init OCR again.
  ASSERT_TRUE(CreateAndInitOCR(mojom::OcrClientType::kTest));
  ASSERT_TRUE(router->IsProcessRunningForTesting(
      screen_ai::ScreenAIServiceRouter::Service::kOCR));

  // Perform OCR.
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
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

  ASSERT_TRUE(CreateAndInitOCR(mojom::OcrClientType::kTest));
  ocr()->DisconnectAnnotator();

  // Perform OCR and get VisualAnnotation.
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr/just_one_letter.png")));
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());

  // Fake library always returns empty results.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  ASSERT_FALSE(perform_future.Get<mojom::VisualAnnotationPtr>()->lines.empty());
#endif

  ocr()->DisconnectAnnotator();

  // Perform OCR and get VisualAnnotation.
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future2;
  ocr()->PerformOCR(bitmap, perform_future2.GetCallback());
  ASSERT_TRUE(perform_future2.Wait());

  // Fake library always returns empty results.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)
  ASSERT_FALSE(
      perform_future2.Get<mojom::VisualAnnotationPtr>()->lines.empty());
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
      public ::testing::WithParamInterface<std::string_view> {
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

  // Returns true if ocr initialization was successful.
  // TODO(crbug.com/378472917): Add a `OpticalCharacterRecognizerTestBase` to
  // avoid redundancy.
  bool CreateAndInitOCR() {
    base::test::TestFuture<bool> init_future;
    ocr_ = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), mojom::OcrClientType::kTest,
        init_future.GetCallback());
    EXPECT_TRUE(init_future.Wait());
    return init_future.Get<bool>();
  }

  scoped_refptr<OpticalCharacterRecognizer> ocr() { return ocr_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<OpticalCharacterRecognizer> ocr_;
};

// If this test fails after updating the library, the failure can be related to
// minor changes in recognition results of the library. The failed cases can be
// checked and if the new result is acceptable. For each test case, the last
// line in the .txt file is the minimum acceptable match and it can be updated.
IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerResultsTest, PerformOCR) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(CreateAndInitOCR());

  // Perform OCR.
  base::FilePath image_path =
      base::FilePath(FILE_PATH_LITERAL("ocr"))
          .AppendASCII(base::StringPrintf("%s.png", GetParam()));
  SkBitmap bitmap = LoadImageFromTestFile(image_path);
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
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
                         testing::ValuesIn(kTestFilenames));

// This test is slow and most probably failing on debug builds and ASAN builds
// which are slower than the other tests.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
#define MAYBE_PerformOCRLargeImage DISABLED_PerformOCRLargeImage
#else
#define MAYBE_PerformOCRLargeImage PerformOCRLargeImage
#endif
IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       MAYBE_PerformOCRLargeImage) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    // crbug.com/417534818
    GTEST_SKIP()
        << "This test is slow and flaky when accessibility is enabled.";
  }
  base::HistogramTester histograms;

  // Since this test processes a huge image, it can be slow and overrun the
  // timeout.
  base::test::ScopedDisableRunLoopTimeout disable_timeout;
  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(CreateAndInitOCR());

  base::FilePath image_path =
      base::FilePath(FILE_PATH_LITERAL("ocr"))
          .Append(FILE_PATH_LITERAL("building_chromium_long.png"));
  SkBitmap bitmap = LoadImageFromTestFile(image_path);
  base::test::TestFuture<mojom::VisualAnnotationPtr> perform_future;
  ocr()->PerformOCR(bitmap, perform_future.GetCallback());
  ASSERT_TRUE(perform_future.Wait());
  auto& results = perform_future.Get<mojom::VisualAnnotationPtr>();

  // Since OCR downsamples large images, the content of this image becomes quite
  // small and unreadable, hence nothing is recognized.
  EXPECT_FALSE(results->lines.size());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.OCR.Downsampled.ClientType", 1);
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRMultipleFilesOneByOne) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(CreateAndInitOCR());

  for (std::string_view& file_name : kTestFilenames) {
    base::FilePath image_path =
        base::FilePath(FILE_PATH_LITERAL("ocr"))
            .AppendASCII(base::StringPrintf("%s.png", file_name));
    SkBitmap bitmap = LoadImageFromTestFile(image_path);
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr()->PerformOCR(bitmap, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    auto& results = future.Get<mojom::VisualAnnotationPtr>();
    EXPECT_TRUE(results->lines.size());
  }
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRMultipleFilesNoWaitBetween) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(CreateAndInitOCR());

  SkBitmap bitmaps[kTestFilenamesCount];
  {
    auto* bitmap = std::begin(bitmaps);
    for (std::string_view& file_name : kTestFilenames) {
      base::FilePath image_path =
          base::FilePath(FILE_PATH_LITERAL("ocr"))
              .AppendASCII(base::StringPrintf("%s.png", file_name));
      *bitmap = LoadImageFromTestFile(image_path);
      bitmap = std::next(bitmap);
    }
  }

  base::test::TestFuture<mojom::VisualAnnotationPtr>
      futures[kTestFilenamesCount];
  {
    auto* future = std::begin(futures);
    for (SkBitmap& bitmap : bitmaps) {
      ocr()->PerformOCR(bitmap, future->GetCallback());
      future = std::next(future);
    }
  }

  // Verify that OCR is not considered busy as there is only one connected
  // client.
  {
    base::test::TestFuture<bool> future;

    ocr()->IsOCRBusy(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get<bool>());
  }

  for (auto& future : futures) {
    ASSERT_TRUE(future.Wait());
    auto& results = future.Get<mojom::VisualAnnotationPtr>();
    EXPECT_TRUE(results->lines.size());
  }
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRMultipleClientsNoWaitBetween) {
  base::HistogramTester histograms;
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Create multiple OCR clients.
  scoped_refptr<OpticalCharacterRecognizer> ocr_clients[kTestFilenamesCount];
  {
    base::test::TestFuture<bool> futures[kTestFilenamesCount];
    {
      auto* future = std::begin(futures);
      auto* ocr = std::begin(ocr_clients);
      for (int i = 0; i < kTestFilenamesCount; i++) {
        (*ocr) = OpticalCharacterRecognizer::CreateWithStatusCallback(
            browser()->profile(), mojom::OcrClientType::kTest,
            future->GetCallback());
        future = std::next(future);
        ocr = std::next(ocr);
      }
    }

    for (auto& future : futures) {
      ASSERT_TRUE(future.Wait());
      ASSERT_TRUE(future.Get<bool>());
    }
  }

  // Load files.
  SkBitmap bitmaps[kTestFilenamesCount];
  {
    auto* bitmap = std::begin(bitmaps);
    for (std::string_view& file_name : kTestFilenames) {
      base::FilePath image_path =
          base::FilePath(FILE_PATH_LITERAL("ocr"))
              .AppendASCII(base::StringPrintf("%s.png", file_name));
      *bitmap = LoadImageFromTestFile(image_path);
      bitmap = std::next(bitmap);
    }
  }

  // Perform OCR on all client without waiting.
  base::test::TestFuture<mojom::VisualAnnotationPtr>
      futures[kTestFilenamesCount];
  {
    auto* future = std::begin(futures);
    auto* ocr = std::begin(ocr_clients);
    for (SkBitmap& bitmap : bitmaps) {
      (*ocr)->PerformOCR(bitmap, future->GetCallback());
      future = std::next(future);
      ocr = std::next(ocr);
    }
  }

  // Verify all got results.
  for (auto& future : futures) {
    ASSERT_TRUE(future.Wait());
    auto& results = future.Get<mojom::VisualAnnotationPtr>();
    EXPECT_TRUE(results->lines.size());
  }

  // Disconnect all clients.
  for (auto& ocr : ocr_clients) {
    ocr.reset();
  }

  // Wait for the service to shutdown and store metrics.
  screen_ai::ScreenAIServiceRouter* router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile());
  base::test::TestFuture<void> future;
  WaitForDisconnecting(router, future.GetCallback(), /*remaining_tries=*/3);
  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(router->IsProcessRunningForTesting(
      screen_ai::ScreenAIServiceRouter::Service::kOCR));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Memory use and lifetime measurements should be recorded only once since all
  // tasks were done with the same process.
  histograms.ExpectTotalCount("Accessibility.OCR.Service.LifeTime", 1);

  // Since this metric is recorded at long intervals, ensure the test has been
  // running long enough to record it.
  if (base::TimeTicks::Now() - start_time > kResourceMeasurementInterval * 2) {
    histograms.ExpectTotalCount("Accessibility.OCR.Service.MaxMemoryLoad", 1);
  }

  // Expect no OCR mode change.
  histograms.ExpectUniqueSample("Accessibility.ScreenAI.OCR.ModeSwitch", 0, 1);
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRLightModeEnglish) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SkBitmap bitmap =
      LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL("ocr"))
                                .AppendASCII("simple_text_only_sample.png"));

  scoped_refptr<OpticalCharacterRecognizer> ocr_client;
  {
    base::test::TestFuture<bool> future;
    ocr_client = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), mojom::OcrClientType::kTest,
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    ASSERT_TRUE(future.Get<bool>());
  }

  // Recognize in normal mode and store data.
  int lines_count;
  std::string first_line_text;
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    lines_count = future.Get<mojom::VisualAnnotationPtr>()->lines.size();
    ASSERT_GT(lines_count, 0);
    first_line_text =
        future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line;
  }

  // Set Light mode.
  ocr_client->SetOCRLightMode(true);

  // Recognize in light mode, ensure no change.
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(lines_count,
              future.Get<mojom::VisualAnnotationPtr>()->lines.size());
    EXPECT_EQ(first_line_text,
              future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line);
  }

  // Back to normal mode and still no change.
  ocr_client->SetOCRLightMode(false);
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(lines_count,
              future.Get<mojom::VisualAnnotationPtr>()->lines.size());
    EXPECT_EQ(first_line_text,
              future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line);
  }
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRLightModeChinese) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SkBitmap bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr")).AppendASCII("chinese.png"));

  scoped_refptr<OpticalCharacterRecognizer> ocr_client;
  {
    base::test::TestFuture<bool> future;
    ocr_client = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), mojom::OcrClientType::kTest,
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    ASSERT_TRUE(future.Get<bool>());
  }

  // Recognize in normal mode and store data.
  int lines_count;
  std::string first_line_text;
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    lines_count = future.Get<mojom::VisualAnnotationPtr>()->lines.size();
    ASSERT_GT(lines_count, 0);
    first_line_text =
        future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line;
  }

  // Set Light mode.
  ocr_client->SetOCRLightMode(true);

  // Recognize in light mode. Chinese text is not recognized, however since OCR
  // only has Latin recognizer, it tries to recognize the text using that and
  // does not return empty.
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(lines_count,
              future.Get<mojom::VisualAnnotationPtr>()->lines.size());
    EXPECT_NE(first_line_text,
              future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line);
  }

  // Back to normal mode and correct recognition.
  ocr_client->SetOCRLightMode(false);
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> future;
    ocr_client->PerformOCR(bitmap, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(lines_count,
              future.Get<mojom::VisualAnnotationPtr>()->lines.size());
    EXPECT_EQ(first_line_text,
              future.Get<mojom::VisualAnnotationPtr>()->lines[0]->text_line);
  }
}

IN_PROC_BROWSER_TEST_F(OpticalCharacterRecognizerResultsTest,
                       PerformOCRMultipleClientsLightMode) {
  base::HistogramTester histograms;
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create two OCR clients.
  scoped_refptr<OpticalCharacterRecognizer> ocr_clients[2];
  {
    base::test::TestFuture<bool> futures[2];

    ocr_clients[0] = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), mojom::OcrClientType::kTest,
        futures[0].GetCallback());
    ocr_clients[1] = OpticalCharacterRecognizer::CreateWithStatusCallback(
        browser()->profile(), mojom::OcrClientType::kTest,
        futures[1].GetCallback());

    ASSERT_TRUE(futures[0].Wait());
    ASSERT_TRUE(futures[1].Wait());
    ASSERT_TRUE(futures[0].Get<bool>());
    ASSERT_TRUE(futures[1].Get<bool>());
  }

  // Load files.
  SkBitmap chinese_bitmap = LoadImageFromTestFile(
      base::FilePath(FILE_PATH_LITERAL("ocr")).AppendASCII("chinese.png"));
  SkBitmap english_bitmap =
      LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL("ocr"))
                                .AppendASCII("simple_text_only_sample.png"));

  // Set Light mode on the second client.
  ocr_clients[1]->SetOCRLightMode(true);

  // Send both bitmaps from both clients and wait for completion.
  {
    base::test::TestFuture<mojom::VisualAnnotationPtr> result_futures[4];

    ocr_clients[0]->PerformOCR(english_bitmap, result_futures[0].GetCallback());
    ocr_clients[1]->PerformOCR(english_bitmap, result_futures[1].GetCallback());
    ocr_clients[0]->PerformOCR(chinese_bitmap, result_futures[2].GetCallback());
    ocr_clients[1]->PerformOCR(chinese_bitmap, result_futures[3].GetCallback());
    EXPECT_TRUE(result_futures[3].Wait());

    const auto& english_0 = result_futures[0].Get<mojom::VisualAnnotationPtr>();
    const auto& english_1 = result_futures[1].Get<mojom::VisualAnnotationPtr>();
    const auto& chinese_0 = result_futures[2].Get<mojom::VisualAnnotationPtr>();
    const auto& chinese_1 = result_futures[3].Get<mojom::VisualAnnotationPtr>();

    // Light mode should not affect English text.
    EXPECT_EQ(english_0->lines.size(), english_1->lines.size());
    for (unsigned i = 0; i < english_0->lines.size(); i++) {
      EXPECT_EQ(english_0->lines[i]->text_line, english_1->lines[i]->text_line);
    }

    // Chinese on client 1 is not recognized, however since OCR only has Latin
    // recognizer, it tries to recognize the text using that and does not return
    // empty.
    EXPECT_EQ(chinese_0->lines.size(), chinese_1->lines.size());
    for (unsigned i = 0; i < chinese_0->lines.size(); i++) {
      EXPECT_NE(chinese_0->lines[i]->text_line, chinese_1->lines[i]->text_line);
    }
  }

  // Verify that the both clients consider OCR busy as there is another
  // connected client.
  {
    base::test::TestFuture<bool> features[2];

    ocr_clients[0]->IsOCRBusy(features[0].GetCallback());
    ocr_clients[1]->IsOCRBusy(features[1].GetCallback());

    EXPECT_TRUE(features[0].Wait());
    EXPECT_TRUE(features[1].Wait());
    EXPECT_TRUE(features[0].Get<bool>());
    EXPECT_TRUE(features[1].Get<bool>());
  }

  // Disconnect clients and wait for the service to shutdown and store metrics.
  ocr_clients[0].reset();
  ocr_clients[1].reset();
  screen_ai::ScreenAIServiceRouter* router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile());
  base::test::TestFuture<void> future;
  WaitForDisconnecting(router, future.GetCallback(), /*remaining_tries=*/3);
  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(router->IsProcessRunningForTesting(
      screen_ai::ScreenAIServiceRouter::Service::kOCR));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Verify there has been three switches between light and normal mode. The
  // actual number may differ based on mojo queue handling, but at least one
  // should be recorded.
  histograms.ExpectBucketCount("Accessibility.ScreenAI.OCR.ModeSwitch", 0, 0);
  EXPECT_GT(
      histograms.GetAllSamples("Accessibility.ScreenAI.OCR.ModeSwitch").size(),
      0);
}

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !
        // BUILDFLAG(USE_FAKE_SCREEN_AI)
}  // namespace screen_ai
