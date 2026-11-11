// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace glic {
namespace {

class GlicPasteEligibilityBrowserTest : public GlicBrowserTest {
 public:
  GlicPasteEligibilityBrowserTest() {
    SetGlicPagePath("/glic/test_client/index.html");
    ui::TestClipboard::CreateForCurrentThread();
  }

  ~GlicPasteEligibilityBrowserTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    GlicBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    glic::ResetClipboardEligibilityMonitorForTesting();
    GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  static content::ClipboardEndpoint MakeGuestDestination(
      content::WebContents* glic_guest) {
    return content::ClipboardEndpoint::ForFrame(
        ui::DataTransferEndpoint(glic_guest->GetLastCommittedURL()),
        base::BindLambdaForTesting(
            [glic_guest] { return glic_guest->GetBrowserContext(); }),
        *glic_guest->GetPrimaryMainFrame());
  }

  static std::u16string ReadCurrentClipboardText() {
    return ui::clipboard_test_util::ReadText(
        ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
        /*data_dst=*/nullptr);
  }

  std::optional<content::ClipboardPasteData> SyncCheckPasteEligibility(
      const content::ClipboardEndpoint& source,
      content::WebContents* glic_guest) {
    ui::ClipboardMetadata metadata = {
        .size = 4,
        .format_type = ui::ClipboardFormatType::BitmapType(),
    };

    // Because the test manually calls glic::OnBeforeClipboardCopy without
    // triggering a real OS clipboard write, the global ui::ClipboardObserver
    // never fires. We manually assign the pending eligibility to our test's
    // sequence number.
    glic::SetClipboardEligibilitySeqnoForTesting(metadata.seqno);

    content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

    // Simulate what ChromeContentBrowserClient::IsClipboardPasteAllowedByPolicy
    // does for Glic.
    glic::LogPasteAttempt(source, metadata);
    if (!glic::IsClipboardPasteAllowed(source, destination, metadata)) {
      return std::nullopt;  // Denied
    }

    content::ClipboardPasteData paste_data;
    paste_data.png = {1, 2, 3, 4};
    return paste_data;  // Allowed
  }

  content::WebContents* GetReadyGuest() {
    EXPECT_OK(WaitForGlicClient());
    EXPECT_OK(WaitUntilGuestIsShowing());

    content::WebContents* glic_guest =
        GetOnlyGlicInstance()->host().web_client_contents();
    EXPECT_TRUE(glic_guest != nullptr && IsGlicGuest(glic_guest));
    return glic_guest;
  }

  void InstallBlockedNoticesRecorder(content::WebContents* glic_guest) {
    ASSERT_TRUE(content::ExecJs(glic_guest, R"(
      (async () => {
        window.__capturedBlockedNotices = [];
        await window.client.getInitialized();
        window.client.browser.getDataTransferActionsBlockedNotices().subscribe(
            notice => window.__capturedBlockedNotices.push(notice));
      })()
    )"));
  }

  base::ListValue GetCapturedBlockedNotices(content::WebContents* glic_guest) {
    return content::EvalJs(glic_guest, "window.__capturedBlockedNotices")
        .TakeValue()
        .TakeList();
  }

  bool WaitForBlockedNoticeCount(content::WebContents* glic_guest,
                                 size_t expected_count) {
    return base::test::RunUntil([&]() {
      return GetCapturedBlockedNotices(glic_guest).size() == expected_count;
    });
  }

 private:
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
};

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       AllowPasteFromEligibleTab) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());

  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  content::ClipboardEndpoint source = content::ClipboardEndpoint::ForFrame(
      ui::DataTransferEndpoint(url),
      base::BindLambdaForTesting(
          [source_contents] { return source_contents->GetBrowserContext(); }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_EQ(allowed_data->png, std::vector<uint8_t>({1, 2, 3, 4}));

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                    0);
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       BlockPasteFromIneligibleTab) {
  const GURL url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());

  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  content::ClipboardEndpoint source = content::ClipboardEndpoint::ForFrame(
      ui::DataTransferEndpoint(url),
      base::BindLambdaForTesting(
          [source_contents] { return source_contents->GetBrowserContext(); }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source, glic_guest);
  EXPECT_FALSE(allowed_data.has_value());  // Denied

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      0 /* kPageContextIneligible */, 1);
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest, AllowPasteFromOS) {
  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  // We use an ineligible URL, but since it's an OS copy (no
  // WebContents), it should be allowed anyway because we
  // don't block OS copies.
  const GURL ineligible_url("https://ineligible.example.com/path");
  content::ClipboardEndpoint source_os =
      content::ClipboardEndpoint::ForOutsideChrome(
          ui::DataTransferEndpoint(ineligible_url));
  glic::OnBeforeClipboardCopy(source_os);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source_os, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_EQ(allowed_data->png, std::vector<uint8_t>({1, 2, 3, 4}));  // Allowed

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.OS",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.OS", 0);
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       BlockPasteFromNonTabWebContents) {
  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  // Create a standalone WebContents that is not attached to a Tab.
  std::unique_ptr<content::WebContents> non_tab_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));

  content::ClipboardEndpoint source_non_tab =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(GURL("https://example.com")),
          base::BindLambdaForTesting([&non_tab_contents] {
            return non_tab_contents->GetBrowserContext();
          }),
          *non_tab_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source_non_tab);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source_non_tab, glic_guest);
  EXPECT_FALSE(allowed_data.has_value());  // Denied

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      1 /* kPageContextInvalidated */, 1);
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       BlockPasteAfterSourceTabNavigates) {
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title2.html");

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(
      content::NavigateToURL(active_tab->GetContents(), ineligible_url));

  ASSERT_OK(OpenGlicForActiveTab());

  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  content::ClipboardEndpoint captured_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(ineligible_url),
          base::BindLambdaForTesting(
              [&]() { return source_contents->GetBrowserContext(); }),
          *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(captured_source);

  // Navigate the active tab to the eligible page in the outer sequence loop.
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(captured_source, glic_guest);
  EXPECT_FALSE(allowed_data.has_value());  // Blocked!

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       BlockPasteCrossProfile) {
  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

#if BUILDFLAG(IS_ANDROID)
  // On Android, InProcessBrowserTest::CreateIncognitoBrowser() is unavailable.
  // Create an off-the-record WebContents directly to provide an incognito
  // BrowserContext source for cross-profile paste verification.
  std::unique_ptr<content::WebContents> incognito_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  content::WebContents* source_contents = incognito_contents.get();
#else
  BrowserWindowInterface* incognito_browser =
      InProcessBrowserTest::CreateIncognitoBrowser();
  tabs::TabInterface* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveTab();
  content::WebContents* source_contents = incognito_tab->GetContents();
#endif
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(source_contents, url));
  content::ClipboardEndpoint captured_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(url), base::BindLambdaForTesting([&]() {
            return source_contents->GetBrowserContext();
          }),
          *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(captured_source);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(captured_source, glic_guest);
  EXPECT_FALSE(allowed_data.has_value());  // Blocked!

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      2 /* kCrossProfile */, 1);
}

IN_PROC_BROWSER_TEST_F(
    GlicPasteEligibilityBrowserTest,
    IsClipboardPasteAllowedByPolicyReturnsNulloptWhenDenied) {
  const GURL url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());

  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();

  content::ClipboardEndpoint source = content::ClipboardEndpoint::ForFrame(
      ui::DataTransferEndpoint(url),
      base::BindLambdaForTesting(
          [source_contents] { return source_contents->GetBrowserContext(); }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source);

  ui::ClipboardMetadata metadata = {
      .size = 4,
      .format_type = ui::ClipboardFormatType::BitmapType(),
  };
  glic::SetClipboardEligibilitySeqnoForTesting(metadata.seqno);

  content::ClipboardEndpoint destination = content::ClipboardEndpoint::ForFrame(
      ui::DataTransferEndpoint(glic_guest->GetLastCommittedURL()),
      base::BindLambdaForTesting(
          [glic_guest] { return glic_guest->GetBrowserContext(); }),
      *glic_guest->GetPrimaryMainFrame());

  content::ClipboardPasteData paste_data;
  paste_data.png = {1, 2, 3, 4};

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(
    GlicPasteEligibilityBrowserTest,
    InlineImageSelectionCopyPreservesEligibilityDespiteTrailingEmptyTextWrite) {
  constexpr char kSelectInlineImageJs[] = R"(
    (async () => {
      let box = document.getElementById('img-box');
      if (!box) {
        box = document.createElement('div');
        box.id = 'img-box';
        const img = document.createElement('img');
        img.width = 10;
        img.height = 10;
        img.src = 'data:image/gif;base64,' +
            'R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7';
        box.appendChild(img);
        document.body.appendChild(box);
        await img.decode();
      }
      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.selectAllChildren(box);
      return selection.rangeCount === 1 && !selection.isCollapsed;
    })()
  )";

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // 1. Eligible tab: Blink's WriteSelectionToClipboard sends WriteHtml(...)
  // followed by WriteText(u"") and CommitWrite(). The allowed empty text write
  // must not clear the staged eligibility before CommitWrite().
  {
    source_contents->Focus();
    ASSERT_EQ(true, content::EvalJs(source_contents, kSelectInlineImageJs));
    const ui::ClipboardSequenceNumberToken initial_seqno =
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste);
    source_contents->Copy();
    ASSERT_TRUE(base::test::RunUntil([&] {
      return ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
                 ui::ClipboardBuffer::kCopyPaste) != initial_seqno;
    }));

    std::u16string copied_html;
    std::string src_url;
    uint32_t fragment_start = 0;
    uint32_t fragment_end = 0;
    ui::clipboard_test_util::ReadHTML(ui::Clipboard::GetForCurrentThread(),
                                      ui::ClipboardBuffer::kCopyPaste,
                                      /*data_dst=*/nullptr, &copied_html,
                                      &src_url, &fragment_start, &fragment_end);
    ASSERT_FALSE(copied_html.empty());
    EXPECT_EQ(ReadCurrentClipboardText(), u"");

    base::test::TestFuture<content::ClipboardEndpoint> source_future;
    content::GetSourceClipboardEndpoint(/*data_dst=*/nullptr,
                                        ui::ClipboardBuffer::kCopyPaste,
                                        source_future.GetCallback());
    content::ClipboardEndpoint source = source_future.Get();
    ASSERT_EQ(source.web_contents(), source_contents);

    ui::ClipboardMetadata metadata = {
        .size = copied_html.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::HtmlType(),
        .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste),
    };
    base::HistogramTester histogram_tester;
    content::ClipboardPasteData paste_data;
    paste_data.html = copied_html;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());
    std::optional<content::ClipboardPasteData> allowed = future.Get();
    ASSERT_TRUE(allowed.has_value());
    EXPECT_EQ(allowed->html, copied_html);
    histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                      0);
  }

  // 2. Ineligible tab: the same WriteHtml(...) + WriteText(u"") + CommitWrite()
  // sequence must record false eligibility so paste fails with
  // kPageContextIneligible (0), not kPageContextInvalidated (1).
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));
  {
    source_contents->Focus();
    ASSERT_EQ(true, content::EvalJs(source_contents, kSelectInlineImageJs));
    const ui::ClipboardSequenceNumberToken initial_seqno =
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste);
    source_contents->Copy();
    ASSERT_TRUE(base::test::RunUntil([&] {
      return ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
                 ui::ClipboardBuffer::kCopyPaste) != initial_seqno;
    }));

    std::u16string copied_html;
    std::string src_url;
    uint32_t fragment_start = 0;
    uint32_t fragment_end = 0;
    ui::clipboard_test_util::ReadHTML(ui::Clipboard::GetForCurrentThread(),
                                      ui::ClipboardBuffer::kCopyPaste,
                                      /*data_dst=*/nullptr, &copied_html,
                                      &src_url, &fragment_start, &fragment_end);
    ASSERT_FALSE(copied_html.empty());
    EXPECT_EQ(ReadCurrentClipboardText(), u"");

    base::test::TestFuture<content::ClipboardEndpoint> source_future;
    content::GetSourceClipboardEndpoint(/*data_dst=*/nullptr,
                                        ui::ClipboardBuffer::kCopyPaste,
                                        source_future.GetCallback());
    content::ClipboardEndpoint source = source_future.Get();
    ASSERT_EQ(source.web_contents(), source_contents);

    ui::ClipboardMetadata metadata = {
        .size = copied_html.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::HtmlType(),
        .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste),
    };
    base::HistogramTester histogram_tester;
    content::ClipboardPasteData paste_data;
    paste_data.html = copied_html;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());
    EXPECT_EQ(future.Get(), std::nullopt);
    histogram_tester.ExpectUniqueSample(
        "Glic.Paste.FailedEligibilityReason.Web",
        0 /* kPageContextIneligible */, 1);
  }
}

IN_PROC_BROWSER_TEST_F(GlicPasteEligibilityBrowserTest,
                       EmitsDataTransferActionsBlockedNoticesForBlockedPastes) {
  base::SimpleTestTickClock test_clock;
  test_clock.Advance(base::Minutes(10));
  glic::SetBlockedPasteNoticeClockForTesting(&test_clock);
  base::ScopedClosureRunner reset_clock(base::BindOnce(
      []() { glic::SetBlockedPasteNoticeClockForTesting(nullptr); }));

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  InstallBlockedNoticesRecorder(glic_guest);

  // 1. Eligible web paste does not emit a blocked notice.
  content::WebContents* source_contents = active_tab->GetContents();
  content::ClipboardEndpoint eligible_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(eligible_url),
          base::BindLambdaForTesting([source_contents] {
            return source_contents->GetBrowserContext();
          }),
          *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(eligible_source);
  EXPECT_TRUE(
      SyncCheckPasteEligibility(eligible_source, glic_guest).has_value());

  // 2. OS paste does not emit a blocked notice.
  content::ClipboardEndpoint os_source =
      content::ClipboardEndpoint::ForOutsideChrome(
          ui::DataTransferEndpoint(ineligible_url));
  glic::OnBeforeClipboardCopy(os_source);
  EXPECT_TRUE(SyncCheckPasteEligibility(os_source, glic_guest).has_value());
  EXPECT_TRUE(GetCapturedBlockedNotices(glic_guest).empty());

  // 3. Ineligible page context paste emits {action: PASTE, reason:
  // INELIGIBLE_PAGE_CONTEXT}.
  ASSERT_TRUE(
      content::NavigateToURL(active_tab->GetContents(), ineligible_url));
  content::ClipboardEndpoint ineligible_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(ineligible_url),
          base::BindLambdaForTesting([source_contents] {
            return source_contents->GetBrowserContext();
          }),
          *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(ineligible_source);
  EXPECT_FALSE(
      SyncCheckPasteEligibility(ineligible_source, glic_guest).has_value());
  ASSERT_TRUE(WaitForBlockedNoticeCount(glic_guest, 1u));

  // 4. Non-tab WebContents paste emits {action: PASTE, reason:
  // UNSUPPORTED_OR_EXPIRED_SOURCE}.
  std::unique_ptr<content::WebContents> non_tab_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  content::ClipboardEndpoint non_tab_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(eligible_url),
          base::BindLambdaForTesting([&non_tab_contents] {
            return non_tab_contents->GetBrowserContext();
          }),
          *non_tab_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(non_tab_source);
  EXPECT_FALSE(
      SyncCheckPasteEligibility(non_tab_source, glic_guest).has_value());
  ASSERT_TRUE(WaitForBlockedNoticeCount(glic_guest, 2u));

  // 5. Cross-profile paste emits {action: PASTE, reason: CROSS_PROFILE}.
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<content::WebContents> incognito_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  content::WebContents* cross_profile_contents = incognito_contents.get();
#else
  BrowserWindowInterface* incognito_browser =
      InProcessBrowserTest::CreateIncognitoBrowser();
  content::WebContents* cross_profile_contents =
      incognito_browser->tab_strip_model()->GetActiveTab()->GetContents();
#endif
  ASSERT_TRUE(content::NavigateToURL(cross_profile_contents, eligible_url));
  content::ClipboardEndpoint cross_profile_source =
      content::ClipboardEndpoint::ForFrame(
          ui::DataTransferEndpoint(eligible_url),
          base::BindLambdaForTesting([cross_profile_contents] {
            return cross_profile_contents->GetBrowserContext();
          }),
          *cross_profile_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(cross_profile_source);
  EXPECT_FALSE(
      SyncCheckPasteEligibility(cross_profile_source, glic_guest).has_value());
  ASSERT_TRUE(WaitForBlockedNoticeCount(glic_guest, 3u));

  base::ListValue notices = GetCapturedBlockedNotices(glic_guest);
  ASSERT_EQ(notices.size(), 3u);
  EXPECT_EQ(notices[0].GetDict().FindInt("action"),
            static_cast<int>(mojom::DataTransferAction::kPaste));
  EXPECT_EQ(
      notices[0].GetDict().FindInt("reason"),
      static_cast<int>(
          mojom::DataTransferActionsBlockedReason::kIneligiblePageContext));
  EXPECT_EQ(notices[1].GetDict().FindInt("action"),
            static_cast<int>(mojom::DataTransferAction::kPaste));
  EXPECT_EQ(notices[1].GetDict().FindInt("reason"),
            static_cast<int>(mojom::DataTransferActionsBlockedReason::
                                 kUnsupportedOrExpiredSource));
  EXPECT_EQ(notices[2].GetDict().FindInt("action"),
            static_cast<int>(mojom::DataTransferAction::kPaste));
  EXPECT_EQ(
      notices[2].GetDict().FindInt("reason"),
      static_cast<int>(mojom::DataTransferActionsBlockedReason::kCrossProfile));
}

IN_PROC_BROWSER_TEST_F(
    GlicPasteEligibilityBrowserTest,
    MultiFormatPasteCoalescesNoticeWithinBurstWindowAndReemitsAfterDelay) {
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  glic::SetBlockedPasteNoticeClockForTesting(&test_clock);
  base::ScopedClosureRunner reset_clock(base::BindOnce(
      []() { glic::SetBlockedPasteNoticeClockForTesting(nullptr); }));

  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(
      content::NavigateToURL(active_tab->GetContents(), ineligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  InstallBlockedNoticesRecorder(glic_guest);

  // Copy rich HTML + plain text from the ineligible tab via Blink's real copy
  // pipeline so the clipboard contains multiple formats under a single seqno.
  ASSERT_TRUE(content::ExecJs(source_contents, R"(
    document.body.innerHTML = '<b>blocked html</b> plain text';
    window.getSelection().selectAllChildren(document.body);
  )"));
  source_contents->Copy();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    return ui::clipboard_test_util::IsFormatAvailable(
               clipboard, ui::ClipboardFormatType::HtmlType(),
               ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr) &&
           ui::clipboard_test_util::IsFormatAvailable(
               clipboard, ui::ClipboardFormatType::PlainTextType(),
               ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr);
  }));

  // Focus a rich-text contenteditable target in the Glic guest so a single
  // renderer Paste() triggers multiple per-format ClipboardHost read RPCs
  // (ReadHtml followed by ReadText fallback). When the guest is embedded in
  // an outer WebUI WebContents (kGlicNoWebview disabled), first focus the
  // <webview> and its internal <iframe> (SlimWebview on Android renders the
  // guest <iframe> inside an open shadow root without delegating focus()) so
  // the outer WebContents's focused frame tree points to the guest before
  // focusing #paste_target inside the guest.
  if (content::WebContents* outer_contents =
          glic_guest->GetOuterWebContents()) {
    outer_contents->Focus();
    ASSERT_TRUE(content::ExecJs(outer_contents, R"(
      const webview = document.getElementById('guestFrame');
      if (webview) {
        webview.focus();
        webview.shadowRoot?.querySelector('iframe')?.focus();
      }
    )"));
  }
  ASSERT_TRUE(content::ExecJs(glic_guest, R"(
    const target = document.createElement('div');
    target.id = 'paste_target';
    target.contentEditable = 'true';
    document.body.appendChild(target);
    target.focus();
  )"));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return glic_guest->GetFocusedFrame() == glic_guest->GetPrimaryMainFrame();
  }));

  base::HistogramTester histogram_tester;
  glic_guest->Paste();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Paste.FailedEligibilityReason.Web",
               0 /* kPageContextIneligible */) >= 2;
  }));
  ASSERT_TRUE(WaitForBlockedNoticeCount(glic_guest, 1u));
  EXPECT_EQ("", content::EvalJs(
                    glic_guest,
                    "document.getElementById('paste_target').textContent"));
  EXPECT_EQ(GetCapturedBlockedNotices(glic_guest).size(), 1u);

  // After the 500ms burst window elapses, a second Paste() with the same
  // clipboard sequence number emits a fresh notice. Advance the test clock
  // deterministically without wall-clock sleeping.
  test_clock.Advance(base::Milliseconds(550));

  glic_guest->Paste();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Paste.FailedEligibilityReason.Web",
               0 /* kPageContextIneligible */) >= 4;
  }));
  ASSERT_TRUE(WaitForBlockedNoticeCount(glic_guest, 2u));
  EXPECT_EQ("", content::EvalJs(
                    glic_guest,
                    "document.getElementById('paste_target').textContent"));

  base::ListValue notices = GetCapturedBlockedNotices(glic_guest);
  ASSERT_EQ(notices.size(), 2u);
  for (const auto& entry : notices) {
    EXPECT_EQ(entry.GetDict().FindInt("action"),
              static_cast<int>(mojom::DataTransferAction::kPaste));
    EXPECT_EQ(
        entry.GetDict().FindInt("reason"),
        static_cast<int>(
            mojom::DataTransferActionsBlockedReason::kIneligiblePageContext));
  }
}

}  // namespace
}  // namespace glic
