// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/common/translate_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/mojom/pdf.mojom.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_PDF)
class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  ~FakePdfListener() override = default;

  void SetCaretPosition(const gfx::PointF& position) override {}
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {}
  void SetSelectionBounds(const gfx::PointF& base,
                          const gfx::PointF& extent) override {}
  void GetPdfBytes(uint32_t size_limit, GetPdfBytesCallback callback) override {
    std::move(callback).Run(pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed,
                            std::vector<uint8_t>(), 0);
  }
  void GetPageText(int32_t page_index, GetPageTextCallback callback) override {
    std::move(callback).Run(std::u16string());
  }
  void GetMostVisiblePageIndex(
      GetMostVisiblePageIndexCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }
  void HasMeaningfulText(HasMeaningfulTextCallback callback) override {
    std::move(callback).Run(has_meaningful_text_);
  }
  void HasJavaScript(HasJavaScriptCallback callback) override {
    std::move(callback).Run(has_javascript_);
  }
  void IsPasswordProtected(IsPasswordProtectedCallback callback) override {
    std::move(callback).Run(is_password_protected_);
  }
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  void GetSaveDataBufferHandlerForDrive(
      pdf::mojom::SaveRequestType request_type,
      GetSaveDataBufferHandlerForDriveCallback callback) override {
    std::move(callback).Run(nullptr);
  }
#endif

  void set_has_meaningful_text(bool has_meaningful_text) {
    has_meaningful_text_ = has_meaningful_text;
  }
  void set_has_javascript(bool has_javascript) {
    has_javascript_ = has_javascript;
  }
  void set_is_password_protected(bool is_password_protected) {
    is_password_protected_ = is_password_protected;
  }

 private:
  bool has_meaningful_text_ = false;
  bool has_javascript_ = false;
  bool is_password_protected_ = false;
};

class DummyPDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  DummyPDFDocumentHelperClient() = default;
  ~DummyPDFDocumentHelperClient() override = default;
  void OnDidScroll(const gfx::SelectionBound& start,
                   const gfx::SelectionBound& end) override {}
};
#endif  // BUILDFLAG(ENABLE_PDF)

class ChromeTranslateClientPdfDisabledBrowsertest
    : public InProcessBrowserTest {
 public:
  ChromeTranslateClientPdfDisabledBrowsertest() {
    feature_list_.InitAndDisableFeature(translate::kEnableTranslatePdf);
  }
  ~ChromeTranslateClientPdfDisabledBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ChromeTranslateClient* chrome_translate_client() {
    return ChromeTranslateClient::FromWebContents(web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ChromeTranslateClientPdfEnabledBrowsertest : public InProcessBrowserTest {
 public:
  ChromeTranslateClientPdfEnabledBrowsertest() {
    feature_list_.InitAndEnableFeature(translate::kEnableTranslatePdf);
  }
  ~ChromeTranslateClientPdfEnabledBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ChromeTranslateClient* chrome_translate_client() {
    return ChromeTranslateClient::FromWebContents(web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// When `translate::kEnableTranslatePdf` is disabled, the PDF is not
// translatable.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfDisabledBrowsertest,
                       FalseWhenFeatureDisabled) {
  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// If `pdf::PDFDocumentHelper` is missing, the PDF is not translatable.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       FalseWhenPdfHelperMissing) {
#if BUILDFLAG(ENABLE_PDF)
  EXPECT_EQ(pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents()),
            nullptr);
#endif

  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

#if BUILDFLAG(ENABLE_PDF)
// If the PDF has meaningful text and no JavaScript, it returns true.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       TrueWhenHasMeaningfulText) {
  auto client = std::make_unique<DummyPDFDocumentHelperClient>();
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame(), std::move(client));

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pdf_helper, nullptr);

  // Bind the fake listener.
  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper->SetListener(receiver.BindNewPipeAndPassRemote());

  // Call `CheckIfPdfIsTranslatable`.
  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Simulate `OnDocumentLoadComplete()`. This should trigger the callback.
  pdf_helper->OnDocumentLoadComplete();

  // Wait for the result and verify it.
  EXPECT_TRUE(future.Get());
}

// If the PDF has no meaningful text, it returns false.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       FalseWhenNoMeaningfulText) {
  auto client = std::make_unique<DummyPDFDocumentHelperClient>();
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame(), std::move(client));

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pdf_helper, nullptr);

  FakePdfListener listener;
  listener.set_has_meaningful_text(false);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper->SetListener(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  pdf_helper->OnDocumentLoadComplete();
  EXPECT_FALSE(future.Get());
}

// If the PDF has JavaScript, it returns false.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       FalseWhenHasJavaScript) {
  auto client = std::make_unique<DummyPDFDocumentHelperClient>();
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame(), std::move(client));

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pdf_helper, nullptr);

  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(true);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper->SetListener(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  pdf_helper->OnDocumentLoadComplete();
  EXPECT_FALSE(future.Get());
}

// If `IsPasswordProtected()` returns `true`, `CheckIfPdfIsTranslatable()`
// returns `false`.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       FalseWhenIsPasswordProtected) {
  auto client = std::make_unique<DummyPDFDocumentHelperClient>();
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame(), std::move(client));

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pdf_helper, nullptr);

  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(false);
  listener.set_is_password_protected(true);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper->SetListener(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  pdf_helper->OnDocumentLoadComplete();
  EXPECT_FALSE(future.Get());
}

// If the document is destroyed during a pending load, it should not crash.
IN_PROC_BROWSER_TEST_F(ChromeTranslateClientPdfEnabledBrowsertest,
                       NoCrashWhenDestroyed) {
  auto client = std::make_unique<DummyPDFDocumentHelperClient>();
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame(), std::move(client));

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pdf_helper, nullptr);

  FakePdfListener listener;
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper->SetListener(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<bool> future;
  chrome_translate_client()->CheckIfPdfIsTranslatable(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Navigate to another page, which destroys the document and
  // `pdf::PDFDocumentHelper`
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // The destruction of `pdf::PDFDocumentHelper` should have immediately
  // triggered the callback with false.
  EXPECT_FALSE(future.Get());
}
#endif  // BUILDFLAG(ENABLE_PDF)
