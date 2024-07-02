// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "pdf/loader/document_loader_impl.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace content {

class PdfFindRequestManagerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    // Swap the WebContents's delegate for our test delegate.
    normal_delegate_ = contents()->GetDelegate();
    contents()->SetDelegate(&test_delegate_);
  }

  void TearDownOnMainThread() override {
    // Swap the WebContents's delegate back to its usual delegate.
    contents()->SetDelegate(normal_delegate_);
    normal_delegate_ = nullptr;
  }

 protected:
  // Navigates to |url| and waits for it to finish loading.
  void LoadAndWait(const std::string& url) {
    TestNavigationObserver navigation_observer(contents());
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), embedded_test_server()->GetURL("a.com", url), 1);
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            blink::mojom::FindOptionsPtr options) {
    delegate()->UpdateLastRequest(++last_request_id_);
    contents()->Find(last_request_id_, base::UTF8ToUTF16(search_text),
                     std::move(options), /*skip_delay=*/false);
  }

  WebContents* contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FindTestWebContentsDelegate* delegate() const {
    return static_cast<FindTestWebContentsDelegate*>(contents()->GetDelegate());
  }

  int last_request_id() const { return last_request_id_; }

 private:
  FindTestWebContentsDelegate test_delegate_;
  raw_ptr<WebContentsDelegate> normal_delegate_ = nullptr;

  // The ID of the last find request requested.
  int last_request_id_ = 0;
};

class PdfFindRequestManagerTestWithPdfPartialLoading
    : public PdfFindRequestManagerTest {
 public:
  PdfFindRequestManagerTestWithPdfPartialLoading() {
    feature_list_.InitWithFeatures(
        {chrome_pdf::features::kPdfIncrementalLoading,
         chrome_pdf::features::kPdfPartialLoading},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests searching in a full-page PDF.
// Flaky on Windows ASAN: crbug.com/1030368.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_FindInPDF DISABLED_FindInPDF
#else
#define MAYBE_FindInPDF FindInPDF
#endif
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest, MAYBE_FindInPDF) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_pdf_page.pdf");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  Find("result", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();

  options->new_session = false;
  Find("result", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();

  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);
}

void SendRangeResponse(net::test_server::ControllableHttpResponse* response,
                       const std::string& pdf_contents) {
  int range_start = -1;
  int range_end = -1;
  {
    auto it = response->http_request()->headers.find("Range");
    ASSERT_NE(response->http_request()->headers.end(), it);
    std::string_view range_header = it->second;
    std::string_view kBytesPrefix = "bytes=";
    ASSERT_TRUE(base::StartsWith(range_header, kBytesPrefix));
    range_header.remove_prefix(kBytesPrefix.size());
    auto dash_pos = range_header.find('-');
    ASSERT_NE(std::string::npos, dash_pos);
    ASSERT_LT(0u, dash_pos);
    ASSERT_LT(dash_pos, range_header.size() - 1);
    ASSERT_TRUE(
        base::StringToInt(range_header.substr(0, dash_pos), &range_start));
    ASSERT_TRUE(
        base::StringToInt(range_header.substr(dash_pos + 1), &range_end));
  }
  ASSERT_LT(0, range_start);
  ASSERT_LT(range_start, range_end);
  ASSERT_LT(static_cast<size_t>(range_end), pdf_contents.size());
  int range_length = range_end - range_start + 1;
  response->Send("HTTP/1.1 206 Partial Content\r\n");
  response->Send(base::StringPrintf("Content-Range: bytes %d-%d/%zu\r\n",
                                    range_start, range_end,
                                    pdf_contents.size()));
  response->Send(base::StringPrintf("Content-Length: %d\r\n", range_length));
  response->Send("\r\n");
  response->Send(pdf_contents.substr(range_start, range_length));
  response->Done();
}

// Tests searching in a PDF received in chunks via range-requests.  See also
// https://crbug.com/1027173.
// TODO(crbug.com/40926030): flaky on Linux debug.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_FindInChunkedPDF DISABLED_FindInChunkedPDF
#else
#define MAYBE_FindInChunkedPDF FindInChunkedPDF
#endif
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTestWithPdfPartialLoading,
                       MAYBE_FindInChunkedPDF) {
  constexpr uint32_t kStalledResponseSize =
      chrome_pdf::DocumentLoaderImpl::kDefaultRequestSize + 123;

  // Load contents of a big, linearized pdf test file.
  // See also //content/test/data/linearized.pdf.README file.
  std::string pdf_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking_io;
    base::FilePath content_test_dir;
    ASSERT_TRUE(
        base::PathService::Get(content::DIR_TEST_DATA, &content_test_dir));
    base::FilePath real_pdf_path =
        content_test_dir.AppendASCII("linearized.pdf");
    ASSERT_TRUE(base::ReadFileToString(real_pdf_path, &pdf_contents));
  }
  DCHECK_GT(pdf_contents.size(), kStalledResponseSize);

  // Set up handling of HTTP responses from within the test.
  const char kSimulatedPdfPath[] = "/simulated/chunked.pdf";
  net::test_server::ControllableHttpResponse nav_response(
      embedded_test_server(), kSimulatedPdfPath);
  net::test_server::ControllableHttpResponse range_response1(
      embedded_test_server(), kSimulatedPdfPath);
  net::test_server::ControllableHttpResponse range_response2(
      embedded_test_server(), kSimulatedPdfPath);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("a.com", kSimulatedPdfPath);

  // Kick-off browser-initiated navigation to a PDF file.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestNavigationObserver navigation_observer(web_contents);
  content::NavigationController::LoadURLParams params(pdf_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);

  // Have the test HTTP server handle the 1st request (navigation).  This
  // request is handler in the test, rather than by embedded_test_server, to
  // stall the request after it delivers the first kStalledResponseSize bytes of
  // data (the stalling ensures that the range request will be processed in the
  // next test steps).
  nav_response.WaitForRequest();
  nav_response.Send("HTTP/1.1 200 OK\r\n");
  nav_response.Send("Accept-Ranges: bytes\r\n");
  nav_response.Send(
      base::StringPrintf("Content-Length: %zu\r\n", pdf_contents.size()));
  nav_response.Send("Content-Type: application/pdf\r\n");
  nav_response.Send("Pragma: no-cache\r\n");
  nav_response.Send("Cache-Control: no-cache, no-store, must-revalidate\r\n");
  nav_response.Send("\r\n");
  nav_response.Send(pdf_contents.substr(0, kStalledResponseSize));

  // At this point the navigation should be considered successful (even though
  // we haven't loaded all the bytes of the PDF yet).
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());

  // Have the test handle the 2 range requests (subresource requests initiated
  // by the PDF plugin and proxied through a renderer process for
  // MimeHandlerView extension).  These requests are handled in the test, rather
  // than by embedded_test_server, to verify that we are indeed getting range
  // requests (i.e. this is a sanity check that the test still tests the right
  // thing).
  range_response1.WaitForRequest();
  SendRangeResponse(&range_response1, pdf_contents);
  range_response2.WaitForRequest();
  SendRangeResponse(&range_response2, pdf_contents);

  // Finish the first HTTP response and verify that the PDF has loaded
  // successfully.
  nav_response.Done();
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  // Verify that find-in-page works fine.
  auto options = blink::mojom::FindOptions::New();
  Find("FXCMAP_CMap", options.Clone());
  delegate()->WaitForFinalReply();
  options->new_session = false;
  Find("FXCMAP_CMap", options.Clone());
  delegate()->WaitForFinalReply();
  Find("FXCMAP_CMap", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(15, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);
}

// Tests searching in a page with embedded PDFs. Note that this test, the
// FindInPDF test, and the find tests in web_view_browsertest.cc ensure that
// find-in-page works across GuestViews.
//
// TODO(paulmeyer): Note that this is left disabled for now since
// EnsurePDFHasLoaded() currently does not work for embedded PDFs. This will be
// fixed and enabled in a subsequent patch.
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest, DISABLED_FindInEmbeddedPDFs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_embedded_pdf_page.html");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  options->new_session = false;
  Find("result", options.Clone());
  options->forward = false;
  Find("result", options.Clone());
  Find("result", options.Clone());
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(13, results.number_of_matches);
  EXPECT_EQ(11, results.active_match_ordinal);
}

IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest, FindMissingStringInPDF) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_pdf_page.pdf");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  Find("missing", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);
}

// Tests searching for a word character-by-character, as would typically be
// done by a user typing into the find bar.
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest,
                       CharacterByCharacterFindInPDF) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_pdf_page.pdf");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  Find("r", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();
  Find("re", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();
  Find("res", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();
  Find("resu", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();
  Find("resul", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests that find-in-page results only come for the PDF contents, and not from
// the PDF Viewer's UI.
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest, DoesNotSearchPdfViewerUi) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_pdf_page.pdf");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  Find("pdf", options.Clone());
  delegate()->WaitForFinalReply();

  // The UI contains "find_in_pdf_page.pdf", but that should not generate any
  // results.
  // The contents contains one instance of "pdf", which should show up.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(1, results.number_of_matches);
}

// Regression test for crbug.com/1352097.
IN_PROC_BROWSER_TEST_F(PdfFindRequestManagerTest, SingleResultFindNext) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadAndWait("/find_in_pdf_page.pdf");
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(contents()));

  auto options = blink::mojom::FindOptions::New();
  Find("pdf", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();

  options->new_session = false;
  Find("pdf", options.Clone());
  delegate()->MarkNextReply();
  delegate()->WaitForNextReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(1, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

}  // namespace content
