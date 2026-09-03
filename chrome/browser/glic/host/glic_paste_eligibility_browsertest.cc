// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

bool MockIsEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  if (path.find("ineligible") != std::string::npos ||
      host.find("ineligible") != std::string::npos) {
    return false;
  }
  return true;
}

optimization_guide::StringViewSpan MockGetMeta(
    std::string_view,
    std::string_view,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return optimization_guide::StringViewSpan{.data = nullptr, .size = 0};
}

optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>& frames) {
  bool eligible = true;
  for (const auto& frame : frames) {
    if (frame.path.find("ineligible") != std::string::npos ||
        frame.host.find("ineligible") != std::string::npos) {
      eligible = false;
      break;
    }
  }
  return optimization_guide::PageEligibilityResult{
      .status = eligible ? optimization_guide::PageEligibility::kEligible
                         : optimization_guide::PageEligibility::kIneligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}

bool MockIsEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  if (path.find("ineligible") != std::string::npos ||
      host.find("ineligible") != std::string::npos) {
    return false;
  }
  return true;
}

bool MockShouldReextractPageContext(
    const std::string& host,
    const std::string& path,
    const std::vector<std::string>& updated_meta_tags) {
  return false;
}

optimization_guide::PageContextEligibilityAPI g_test_api = {
    .IsPageContextEligible = &MockIsEligible,
    .IsPageContextEligibleWithAccount = &MockIsEligibleWithAccount,
    .ShouldReextractPageContext = &MockShouldReextractPageContext,
    .GetMetaTagNamesAffectingEligibility = &MockGetMeta,
    .CheckPageEligibility = &MockCheckPageEligibility,
};

class GlicPasteEligibilityBrowserTest : public GlicBrowserTest {
 public:
  GlicPasteEligibilityBrowserTest() = default;

  void SetUp() override {
    optimization_guide::PageContextEligibility::SetForTesting(
        GetTestEligibilityHolder());
    GlicBrowserTest::SetUp();
  }

  void TearDown() override {
    GlicBrowserTest::TearDown();
    optimization_guide::PageContextEligibility::SetForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    GlicBrowserTest::SetUpOnMainThread();
  }

 protected:
  optimization_guide::PageContextEligibility* GetTestEligibilityHolder() {
    static base::NoDestructor<optimization_guide::PageContextEligibility>
        holder(&g_test_api);
    return holder.get();
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

    content::ClipboardEndpoint destination(
        ui::DataTransferEndpoint(glic_guest->GetLastCommittedURL()),
        base::BindLambdaForTesting(
            [glic_guest] { return glic_guest->GetBrowserContext(); }),
        *glic_guest->GetPrimaryMainFrame());

    // Simulate what ChromeContentBrowserClient::IsClipboardPasteAllowedByPolicy
    // does for Glic.
    glic::LogPasteAttempt(source, metadata);
    if (!glic::IsClipboardPasteAllowed(source, destination, metadata)) {
      return content::ClipboardPasteData();  // Denied
    }

    content::ClipboardPasteData paste_data;
    paste_data.png = {1, 2, 3, 4};
    return paste_data;  // Allowed
  }

  content::WebContents* GetReadyGuest() {
    WebUIStateListener listener(&GetOnlyGlicInstance()->host());
    listener.WaitForWebUiState(mojom::WebUiState::kReady);

    content::WebContents* glic_guest =
        GetOnlyGlicInstance()->host().web_client_contents();
    EXPECT_TRUE(glic_guest != nullptr && IsGlicGuest(glic_guest));
    return glic_guest;
  }
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

  content::ClipboardEndpoint source(
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

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(url),
      base::BindLambdaForTesting(
          [source_contents] { return source_contents->GetBrowserContext(); }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_TRUE(allowed_data->png.empty());  // Denied

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
  content::ClipboardEndpoint source_os{
      ui::DataTransferEndpoint(ineligible_url)};
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

  content::ClipboardEndpoint source_non_tab(
      ui::DataTransferEndpoint(GURL("https://example.com")),
      base::BindLambdaForTesting([&non_tab_contents] {
        return non_tab_contents->GetBrowserContext();
      }),
      *non_tab_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(source_non_tab);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(source_non_tab, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_TRUE(allowed_data->png.empty());  // Denied

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

  content::ClipboardEndpoint captured_source(
      ui::DataTransferEndpoint(ineligible_url),
      base::BindLambdaForTesting(
          [&]() { return source_contents->GetBrowserContext(); }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(captured_source);

  // Navigate the active tab to the eligible page in the outer sequence loop.
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(captured_source, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_TRUE(allowed_data->png.empty());  // Blocked!

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
  content::ClipboardEndpoint captured_source(
      ui::DataTransferEndpoint(url), base::BindLambdaForTesting([&]() {
        return source_contents->GetBrowserContext();
      }),
      *source_contents->GetPrimaryMainFrame());
  glic::OnBeforeClipboardCopy(captured_source);

  base::HistogramTester histogram_tester;

  auto allowed_data = SyncCheckPasteEligibility(captured_source, glic_guest);
  EXPECT_TRUE(allowed_data.has_value());
  EXPECT_TRUE(allowed_data->png.empty());  // Blocked!

  histogram_tester.ExpectUniqueSample("Glic.Paste.AttemptedFormat.Web",
                                      2 /* kBitmap */, 1);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      2 /* kCrossProfile */, 1);
}

}  // namespace
}  // namespace glic
