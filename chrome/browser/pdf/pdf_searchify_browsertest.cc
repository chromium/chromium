// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/pdf_features.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

// Parameter: Searchify (ScreenAI OCR) availability.
class PDFSearchifyTest : public PDFExtensionTestBase,
                         public screen_ai::ScreenAIInstallState::Observer,
                         public ::testing::WithParamInterface<bool> {
 public:
  PDFSearchifyTest() = default;

  bool IsSearchifyActive() const { return GetParam(); }

  // PDFExtensionTestBase:
  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    if (IsSearchifyActive()) {
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
    } else {
      // Set an observer to mark download as failed when requested.
      component_download_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }

  // PDFExtensionTestBase:
  void TearDownOnMainThread() override {
    component_download_observer_.Reset();
    PDFExtensionTestBase::TearDownOnMainThread();
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
      enabled.push_back({chrome_pdf::features::kPdfSearchify, {}});
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
  std::u16string GetPageText(bool repeat_until_has_text) {
    auto* helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(GetActiveWebContents());
    if (!helper) {
      ADD_FAILURE() << "PDFDocumentHelper not found.";
      return u"";
    }

    std::u16string result;
    EXPECT_TRUE(
        base::test::RunUntil([&helper, &repeat_until_has_text, &result]() {
          base::test::TestFuture<const std::u16string&> future;
          helper->GetPageText(0, future.GetCallback());
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

IN_PROC_BROWSER_TEST_P(PDFSearchifyTest, HelloWorld) {
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/hello-world-in-image.pdf")));

  std::u16string page_text = GetPageText(/*repeat_until_has_text=*/
                                         IsSearchifyActive());

  EXPECT_EQ(page_text, IsSearchifyActive() ? u"Hello, world!" : u"");
}

// TODO(crbug.com/360803943): Add metrics tests similar to
// `PdfOcrIntegrationTest`.

// TODO(crbug.com/360803943): Consider adding save tests like
// pdf_extension_download_test.cc

// TODO(crbug.com/360803943): Add multi-page tests.
