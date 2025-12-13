// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace {

bool IsScreenReaderEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceRendererAccessibility);
}

}  // namespace

// Parameter: Searchify (ScreenAI OCR) availability.
class PDFSearchifyTest
    : public InteractiveFeaturePromoTestMixin<PDFExtensionTestBase>,
      public screen_ai::ScreenAIInstallState::Observer,
      public ::testing::WithParamInterface<bool> {
 public:
  PDFSearchifyTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPdfSearchifyFeature})) {}

  bool IsSearchifyActive() const { return GetParam(); }

  // InteractiveFeaturePromoTestMixin:
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTestMixin::SetUpOnMainThread();

    if (IsSearchifyActive()) {
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
    } else {
      // Set an observer to mark download as failed when requested.
      component_download_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }

  // InteractiveFeaturePromoTestMixin:
  void TearDown() override {
    // `PDFExtensionTestBase`'s feature list is nested in
    // `InteractiveFeaturePromoTestMixin`'s feature list and is initialized
    // after that. `InteractiveFeaturePromoTestMixin` resets the feature list in
    // `TearDown` but `PDFExtensionTestBase` does not do so and keeps it until
    // destruction.
    // As nested feature lists are expected to be reset in the reverse order of
    // their initialization, the feature list of `PDFExtensionTestBase` is reset
    // here.
    ResetFeatureList();
    InteractiveFeaturePromoTestMixin::TearDown();
  }

  // InteractiveFeaturePromoTestMixin:
  void TearDownOnMainThread() override {
    component_download_observer_.Reset();
    InteractiveFeaturePromoTestMixin::TearDownOnMainThread();
  }

  // ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    CHECK(!IsSearchifyActive());
    if (state == screen_ai::ScreenAIInstallState::State::kDownloading) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            screen_ai::ScreenAIInstallState::GetInstance()->SetState(
                screen_ai::ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

 protected:
  // PDFExtensionTestBase:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    if (IsSearchifyActive()) {
      enabled.push_back({::features::kScreenAITestMode, {}});
      enabled.push_back({ax::mojom::features::kScreenAIOCREnabled, {}});
    }
    return enabled;
  }

  // PDFExtensionTestBase:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    auto disabled = PDFExtensionTestBase::GetDisabledFeatures();
    if (!IsSearchifyActive()) {
      disabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }
    return disabled;
  }

  // Searchify may be slow, so if the test expects text, `repeat_until_has_text`
  // should be set to true to repeat getting page text until it's not empty.
  std::u16string GetPageText(int32_t page_index, bool repeat_until_has_text) {
    auto* helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(GetActiveWebContents());
    if (!helper) {
      ADD_FAILURE() << "PDFDocumentHelper not found.";
      return u"";
    }

    std::u16string result;
    EXPECT_TRUE(base::test::RunUntil(
        [&helper, &page_index, &repeat_until_has_text, &result]() {
          base::test::TestFuture<const std::u16string&> future;
          helper->GetPageText(page_index, future.GetCallback());
          EXPECT_TRUE(future.Wait());
          result = future.Take();
          return !result.empty() || !repeat_until_has_text;
        }));
    return result;
  }

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_download_observer_{this};
};

// If a working library does not exist, just try when library is not available.
INSTANTIATE_TEST_SUITE_P(All,
                         PDFSearchifyTest,
#if BUILDFLAG(USE_FAKE_SCREEN_AI)
                         testing::Values(false)
#else
                         testing::Bool()
#endif
);

// TODO(crbug.com/406839385): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HelloWorld DISABLED_HelloWorld
#else
#define MAYBE_HelloWorld HelloWorld
#endif
IN_PROC_BROWSER_TEST_P(PDFSearchifyTest, MAYBE_HelloWorld) {
  base::HistogramTester histograms;
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/hello-world-in-image.pdf")));

  std::u16string page_text =
      GetPageText(/*page_idex=*/0, /*repeat_until_has_text=*/
                  IsSearchifyActive());

  EXPECT_EQ(page_text, IsSearchifyActive() ? u"Hello, world!" : u"");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histograms.ExpectUniqueSample("PDF.PageHasText", false, 1);

  // `ScreenReaderModeEnabled` is recorded only when searchify is active.
  histograms.ExpectUniqueSample(
      "Accessibility.ScreenAI.Searchify.ScreenReaderModeEnabled",
      IsScreenReaderEnabled(), IsSearchifyActive());
}

// TODO(crbug.com/406839385): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultiPage DISABLED_MultiPage
#else
#define MAYBE_MultiPage MultiPage
#endif
IN_PROC_BROWSER_TEST_P(PDFSearchifyTest, MAYBE_MultiPage) {
  base::HistogramTester histograms;
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/inaccessible-text-in-three-page.pdf")));

  static constexpr std::u16string_view kExpectedTexts[3] = {
      u"Hello, world!", u"Paragraph 1 on Page 2Paragraph 2 on Page 2",
      u"Paragraph 1 on Page 3Paragraph 2 on Page 3"};

  int page_index = 0;
  for (const auto& expected_text : kExpectedTexts) {
    std::u16string page_text =
        GetPageText(page_index++, /*repeat_until_has_text=*/
                    IsSearchifyActive());
    EXPECT_EQ(page_text, IsSearchifyActive() ? expected_text : u"");
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histograms.ExpectUniqueSample("PDF.PageHasText", false, 3);

  // `ScreenReaderModeEnabled` is recorded only when searchify is active and is
  // recorded only once for each PDF.
  histograms.ExpectUniqueSample(
      "Accessibility.ScreenAI.Searchify.ScreenReaderModeEnabled",
      IsScreenReaderEnabled(), IsSearchifyActive() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(PDFSearchifyTest, InProductHelp) {
  if (!IsSearchifyActive()) {
    GTEST_SKIP() << "IPH is only shown when searchify is active.";
  }

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/hello-world-in-image.pdf")));

  auto* const user_education = BrowserUserEducationInterface::From(browser());
  EXPECT_TRUE(base::test::RunUntil([&user_education]() {
    return user_education->IsFeaturePromoQueued(
               feature_engagement::kIPHPdfSearchifyFeature) ||
           user_education->IsFeaturePromoActive(
               feature_engagement::kIPHPdfSearchifyFeature);
  }));
}

// TODO(crbug.com/382610226): Add combined save test for ink and searchify.

// TODO(crbug.com/382610226): Add text selection test for PDFs with rotated page
// or image.

// TODO(crbug.com/382610226): Consider adding save tests like
// pdf_extension_download_test.cc
