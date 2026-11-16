// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PDF)

namespace glic {
namespace {

// Enables the FILE_ATTACHED enterprise connector, with a fake standing in for
// the content analysis service.
class GlicPdfContentAnalysisBrowserTest : public GlicBrowserTest {
 public:
  using FakeDelegate = enterprise_connectors::test::FakeContentAnalysisDelegate;

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    FakeDelegate::ResetStaticDialogFlagsAndTotalRequestsCount();
    FakeDelegate::SetResponseDelay(base::Milliseconds(0));
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-dm-token"));
    enterprise_connectors::test::SetAnalysisConnector(
        GetProfile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
        R"({
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1,
          "minimum_data_size": 1
        })");
  }

  void TearDownOnMainThread() override {
    enterprise_connectors::ContentAnalysisDelegate::EnableUIAfterTesting();
    enterprise_connectors::ContentAnalysisDelegate::ResetFactoryForTesting();
    FakeDelegate::SetResponseDelay(base::Milliseconds(0));
    FakeDelegate::ResetStaticDialogFlagsAndTotalRequestsCount();
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Makes scans report `response`, recording the scanned file's path.
  void SetScanResponse(
      enterprise_connectors::ContentAnalysisResponse response) {
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(&FakeDelegate::Create, base::DoNothing(),
                            base::BindLambdaForTesting(
                                [this, response](const std::string& contents,
                                                 const base::FilePath& path) {
                                  scanned_path_ = path;
                                  return response;
                                }),
                            "fake-dm-token"));
  }

  // Shows a PDF in the active tab and opens Glic with that tab pinned.
  void OpenGlicOnPdfTab(tabs::TabInterface** tab, GlicInstanceImpl** instance) {
    *tab = GetTabListInterface()->GetActiveTab();
    NavigateTab(**tab, embedded_test_server()->GetURL("/pdf/test.pdf"));
    ASSERT_TRUE(
        pdf_extension_test_util::EnsurePDFHasLoaded((*tab)->GetContents()));
    ASSERT_OK_AND_ASSIGN(*instance, OpenGlicForActiveTab());
    ASSERT_OK(WaitForGlicClient(*instance));
    (*instance)->GetSharingManagerInternal().PinTabs({(*tab)->GetHandle()});
  }

  static mojom::TabContextOptionsPtr PdfOptions() {
    auto options = mojom::TabContextOptions::New();
    options->pdf_data = true;
    options->pdf_size_limit = 100000;
    return options;
  }

  base::FilePath scanned_path_;
};

IN_PROC_BROWSER_TEST_F(GlicPdfContentAnalysisBrowserTest,
                       BlockVerdictOmitsPdf) {
  tabs::TabInterface* tab = nullptr;
  GlicInstanceImpl* instance = nullptr;
  ASSERT_NO_FATAL_FAILURE(OpenGlicOnPdfTab(&tab, &instance));
  enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
  SetScanResponse(FakeDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
      "block-rule",
      enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
          BLOCK));

  auto options = PdfOptions();
  options->inner_text = true;
  options->inner_text_bytes_limit = 1000;
  options->annotated_page_content = true;
  base::test::TestFuture<GlicGetContextResult> future;
  instance->GetSharingManagerInternal().GetContextFromTab(
      tab->GetHandle(), *options, future.GetCallback());
  GlicGetContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_tab_context());
  const auto& tab_context = result.value()->get_tab_context();
  EXPECT_FALSE(tab_context->pdf_document_data);
  // Only the PDF is omitted; the rest of the context is still returned.
  EXPECT_TRUE(tab_context->web_page_data);
  EXPECT_TRUE(tab_context->annotated_page_data);

  // The bytes were scanned from a temporary file, which is deleted afterwards.
  ASSERT_FALSE(scanned_path_.empty());
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !base::PathExists(scanned_path_); }));
}

IN_PROC_BROWSER_TEST_F(GlicPdfContentAnalysisBrowserTest,
                       AllowVerdictKeepsPdf) {
  tabs::TabInterface* tab = nullptr;
  GlicInstanceImpl* instance = nullptr;
  ASSERT_NO_FATAL_FAILURE(OpenGlicOnPdfTab(&tab, &instance));
  enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
  SetScanResponse(FakeDelegate::SuccessfulResponse({"dlp"}));

  base::test::TestFuture<GlicGetContextResult> future;
  instance->GetSharingManagerInternal().GetContextFromTab(
      tab->GetHandle(), *PdfOptions(), future.GetCallback());
  GlicGetContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_tab_context());
  const auto& tab_context = result.value()->get_tab_context();
  ASSERT_TRUE(tab_context->pdf_document_data);
  EXPECT_TRUE(tab_context->pdf_document_data->pdf_data.has_value());
}

// With `block_until_verdict` set to 0, the PDF is returned immediately while
// the temporary file stays alive until the background scan finishes and cleans
// it up.
IN_PROC_BROWSER_TEST_F(GlicPdfContentAnalysisBrowserTest,
                       NonBlockingScanAllowsImmediatelyAndScansInBackground) {
  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      R"({
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["dlp"]
          }
        ],
        "block_until_verdict": 0,
        "minimum_data_size": 1
      })");
  tabs::TabInterface* tab = nullptr;
  GlicInstanceImpl* instance = nullptr;
  ASSERT_NO_FATAL_FAILURE(OpenGlicOnPdfTab(&tab, &instance));
  FakeDelegate::SetResponseDelay(base::Milliseconds(50));
  SetScanResponse(FakeDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
      "block-rule",
      enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
          BLOCK));

  base::test::TestFuture<GlicGetContextResult> future;
  instance->GetSharingManagerInternal().GetContextFromTab(
      tab->GetHandle(), *PdfOptions(), future.GetCallback());
  GlicGetContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_tab_context());
  const auto& tab_context = result.value()->get_tab_context();
  ASSERT_TRUE(tab_context->pdf_document_data);
  EXPECT_TRUE(tab_context->pdf_document_data->pdf_data.has_value());

  // The background scan still reads the temporary file and deletes it once
  // analysis completes.
  ASSERT_TRUE(base::test::RunUntil([&]() { return !scanned_path_.empty(); }));
  EXPECT_EQ(FakeDelegate::GetTotalAnalysisRequestsCount(), 1);
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !base::PathExists(scanned_path_); }));
}

// With the scan dialog enabled, the scan is abandoned without a verdict if the
// tab closes while it is in flight. The context request must still complete.
IN_PROC_BROWSER_TEST_F(GlicPdfContentAnalysisBrowserTest,
                       TabClosedDuringScanCompletesRequest) {
  tabs::TabInterface* tab = nullptr;
  GlicInstanceImpl* instance = nullptr;
  ASSERT_NO_FATAL_FAILURE(OpenGlicOnPdfTab(&tab, &instance));
  // Keep the scan pending until the tab is closed.
  FakeDelegate::SetResponseDelay(base::Hours(1));
  SetScanResponse(FakeDelegate::SuccessfulResponse({"dlp"}));

  base::test::TestFuture<GlicGetContextResult> future;
  instance->GetSharingManagerInternal().GetContextFromTab(
      tab->GetHandle(), *PdfOptions(), future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      []() { return FakeDelegate::GetTotalAnalysisRequestsCount() > 0; }));
  EXPECT_FALSE(future.IsReady());

  // Add a new tab to keep the browser alive before closing the PDF tab.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  tab->Close();

  EXPECT_FALSE(future.Take().has_value());
}

}  // namespace
}  // namespace glic

#endif  // BUILDFLAG(ENABLE_PDF)
