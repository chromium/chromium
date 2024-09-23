// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/site_isolation/chrome_site_per_process_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/test/shell_test_api.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif

namespace {

class RedirectObserver : public content::WebContentsObserver {
 public:
  explicit RedirectObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        transition_(ui::PageTransition::PAGE_TRANSITION_LINK) {}

  RedirectObserver(const RedirectObserver&) = delete;
  RedirectObserver& operator=(const RedirectObserver&) = delete;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;
    transition_ = navigation_handle->GetPageTransition();
    redirects_ = navigation_handle->GetRedirectChain();
  }

  void WebContentsDestroyed() override {
    // Make sure we don't close the tab while the observer is in scope.
    // See http://crbug.com/314036.
    FAIL() << "WebContents closed during navigation (http://crbug.com/314036).";
  }

  ui::PageTransition transition() const { return transition_; }
  const std::vector<GURL> redirects() const { return redirects_; }

 private:
  ui::PageTransition transition_;
  std::vector<GURL> redirects_;
};

}  // namespace

class SitePerProcessHighDPIExpiredCertBrowserTest
    : public ChromeSitePerProcessTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIExpiredCertBrowserTest()
      : https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  }

  net::EmbeddedTestServer* expired_cert_test_server() {
    return &https_server_expired_;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSitePerProcessTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }

  void SetUpOnMainThread() override {
    ChromeSitePerProcessTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server_expired_.Start());
  }

 private:
  net::EmbeddedTestServer https_server_expired_;
};

double GetFrameDeviceScaleFactor(const content::ToRenderFrameHost& adapter) {
  const char kGetFrameDeviceScaleFactor[] = "window.devicePixelRatio;";
  return content::EvalJs(adapter, kGetFrameDeviceScaleFactor).ExtractDouble();
}

// Flaky on Windows 10. http://crbug.com/700150
#if BUILDFLAG(IS_WIN)
#define MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor \
  DISABLED_InterstitialLoadsWithCorrectDeviceScaleFactor
#else
#define MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor \
  InterstitialLoadsWithCorrectDeviceScaleFactor
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIExpiredCertBrowserTest,
                       MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  EXPECT_EQ(SitePerProcessHighDPIExpiredCertBrowserTest::kDeviceScaleFactor,
            GetFrameDeviceScaleFactor(
                browser()->tab_strip_model()->GetActiveWebContents()));

  // Navigate to page with expired cert.
  GURL bad_cert_url(
      expired_cert_test_server()->GetURL("c.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_cert_url));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* interstitial_frame_host;

  interstitial_frame_host = active_web_contents->GetPrimaryMainFrame();

  EXPECT_EQ(SitePerProcessHighDPIExpiredCertBrowserTest::kDeviceScaleFactor,
            GetFrameDeviceScaleFactor(interstitial_frame_host));
}

// Verify that browser shutdown path works correctly when there's a
// RenderFrameProxyHost for a child frame.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, RenderFrameProxyHostShutdown) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames_remote_and_local.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
}

// Verify that origin replication allows JS access to localStorage, database,
// and FileSystem APIs.  These features involve a check on the
// WebSecurityOrigin of the topmost WebFrame in ContentSettingsObserver, and
// this test ensures this check works when the top frame is remote.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       OriginReplicationAllowsAccessToStorage) {
  // Navigate to a page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate subframe cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", cross_site_url));

  // Find the subframe's RenderFrameHost.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(cross_site_url, frame_host->GetLastCommittedURL());
  EXPECT_TRUE(frame_host->IsCrossProcessSubframe());

  // Check that JS storage APIs can be accessed successfully.
  EXPECT_TRUE(content::ExecJs(frame_host, "localStorage['foo'] = 'bar'"));
  EXPECT_EQ(content::EvalJs(frame_host, "localStorage['foo'];"), "bar");
  EXPECT_EQ(true, EvalJs(frame_host, "!!indexedDB.open('testdb', 2);"));
  EXPECT_TRUE(ExecJs(frame_host,
                     "window.webkitRequestFileSystem("
                     "window.TEMPORARY, 1024, function() {});"));
}

// Ensure that creating a plugin in a cross-site subframe doesn't crash.  This
// involves querying content settings from the renderer process and using the
// top frame's origin as one of the parameters.  See https://crbug.com/426658.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PluginWithRemoteTopFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/chrome/test/data/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate subframe to a page with a Flash object.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url = embedded_test_server()->GetURL(
      "b.com", "/chrome/test/data/flash_object.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));
}

// Verify that ctrl-click of an anchor targeting a remote frame works (i.e. that
// it opens the link in a new tab).  See also https://crbug.com/647772.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       AnchorCtrlClickWhenTargetIsCrossSite) {
  // Navigate to anchor_targeting_remote_frame.html.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/anchor_targeting_remote_frame.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Verify that there is only 1 active tab (with the right contents committed).
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(main_url, main_contents->GetLastCommittedURL());

  // Ctrl-click the anchor/link in the page.
  content::WebContentsAddedObserver new_tab_observer;
#if BUILDFLAG(IS_MAC)
  std::string new_tab_click_script = "simulateClick({ metaKey: true });";
#else
  std::string new_tab_click_script = "simulateClick({ ctrlKey: true });";
#endif
  EXPECT_TRUE(ExecJs(main_contents, new_tab_click_script));

  // Wait for a new tab to appear (the whole point of this test).
  content::WebContents* new_contents = new_tab_observer.GetWebContents();

  // Verify that the new tab has the right contents and is in the right, new
  // place in the tab strip.
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(new_contents, browser()->tab_strip_model()->GetWebContentsAt(1));
  GURL expected_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_EQ(expected_url, new_contents->GetLastCommittedURL());
}

#if BUILDFLAG(ENABLE_PDF)
class ChromeSitePerProcessGuestViewPDFTest : public ChromeSitePerProcessTest {
 public:
  ChromeSitePerProcessGuestViewPDFTest() : test_guest_view_manager_(nullptr) {
    feature_list()->Reset();
    feature_list()->InitAndDisableFeature(chrome_pdf::features::kPdfOopif);
  }

  ChromeSitePerProcessGuestViewPDFTest(
      const ChromeSitePerProcessGuestViewPDFTest&) = delete;
  ChromeSitePerProcessGuestViewPDFTest& operator=(
      const ChromeSitePerProcessGuestViewPDFTest&) = delete;

  ~ChromeSitePerProcessGuestViewPDFTest() override = default;

  void SetUpOnMainThread() override {
    ChromeSitePerProcessTest::SetUpOnMainThread();
    test_guest_view_manager_ = factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                  ->CreateGuestViewManagerDelegate());
  }

 protected:
  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager, DanglingUntriaged>
      test_guest_view_manager_;
};

// This test verifies that when navigating an OOPIF to a page with <embed>-ed
// PDF, the guest is properly created, and by removing the embedder frame, the
// guest is properly destroyed (https://crbug.com/649856).
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessGuestViewPDFTest,
                       EmbeddedPDFInsideCrossOriginFrame) {
  // Navigate to a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Initially, no guests are created.
  EXPECT_EQ(0U, test_guest_view_manager()->num_guests_created());

  // Navigate subframe to a cross-site page with an embedded PDF.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url =
      embedded_test_server()->GetURL("b.com", "/page_with_embedded_pdf.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Wait until the guest for PDF is created.
  auto* guest_view = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);

  auto* primary_main_frame = active_web_contents->GetPrimaryMainFrame();
  ASSERT_NE(primary_main_frame, guest_view->GetGuestMainFrame());

  // Now detach the frame and observe that the guest is destroyed.
  EXPECT_TRUE(
      ExecJs(primary_main_frame,
             "document.body.removeChild(document.querySelector('iframe'));"));
  test_guest_view_manager()->WaitForLastGuestDeleted();

  EXPECT_EQ(0U, test_guest_view_manager()->GetCurrentGuestCount());
}

class ChromeSitePerProcessOopifPDFTest : public ChromeSitePerProcessTest {
 public:
  ChromeSitePerProcessOopifPDFTest() {
    feature_list()->Reset();
    feature_list()->InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
  }

  ChromeSitePerProcessOopifPDFTest(const ChromeSitePerProcessOopifPDFTest&) =
      delete;
  ChromeSitePerProcessOopifPDFTest& operator=(
      const ChromeSitePerProcessOopifPDFTest&) = delete;

  ~ChromeSitePerProcessOopifPDFTest() override = default;

  // Return value could be nullptr.
  pdf::PdfViewerStreamManager* GetPdfViewerStreamManager() {
    return pdf::PdfViewerStreamManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Return value is always non-nullptr. This should only be called after a PDF
  // navigation occurs in the active `content::WebContents`.
  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager() {
    return factory_.GetTestPdfViewerStreamManager(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 private:
  pdf::TestPdfViewerStreamManagerFactory factory_;
};

// This test verifies that when navigating an OOPIF to a page with <embed>-ed
// PDF, the PDF viewer stream manager is properly created, and by removing the
// embedder frame, the stream manager is properly destroyed.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessOopifPDFTest,
                       EmbeddedPDFInsideCrossOriginFrame) {
  // Navigate to a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Initially, the stream manager shouldn't be created.
  EXPECT_FALSE(GetPdfViewerStreamManager());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate subframe to a cross-site page with an embedded PDF.
  GURL frame_url =
      embedded_test_server()->GetURL("b.com", "/page_with_embedded_pdf.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Wait until the PDF is fully loaded.
  content::RenderFrameHost* subframe_main_host =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe_main_host);
  content::RenderFrameHost* embedder_host = ChildFrameAt(subframe_main_host, 0);
  ASSERT_TRUE(embedder_host);
  ASSERT_TRUE(
      GetTestPdfViewerStreamManager()->WaitUntilPdfLoaded(embedder_host));

  // The primary main frame shouldn't be the PDF embedder and shouldn't have a
  // PDF stream.
  auto* primary_main_frame = active_web_contents->GetPrimaryMainFrame();
  ASSERT_FALSE(
      GetTestPdfViewerStreamManager()->GetStreamContainer(primary_main_frame));

  // Now detach the frame and observe that the stream manager is destroyed.
  EXPECT_TRUE(
      ExecJs(primary_main_frame,
             "document.body.removeChild(document.querySelector('iframe'));"));

  EXPECT_FALSE(GetPdfViewerStreamManager());
}

// Check that navigating to a PDF and then trying to access localStorage or
// sessionStorage in the context of the PDF document fails gracefully and
// doesn't lead to a renderer kill. PDF documents don't access these interfaces
// directly, but the access could still happen via DevTools.  See
// https://crbug.com/357014503.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessOopifPDFTest,
                       AccessStorageInPDFDocument) {
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(pdf_url, web_contents->GetLastCommittedURL());
  ASSERT_TRUE(GetTestPdfViewerStreamManager()->WaitUntilPdfLoaded(
      web_contents->GetPrimaryMainFrame()));

  // The PDF document should be in the grandchild frame, embedded in the PDF
  // viewer extension frame.
  content::RenderFrameHost* pdf_extension_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(pdf_extension_frame);
  content::RenderFrameHost* pdf_frame =
      content::ChildFrameAt(pdf_extension_frame, 0);
  ASSERT_TRUE(pdf_frame);
  EXPECT_TRUE(pdf_frame->GetProcess()->IsPdf());

  // When accessed from the PDF document, both localStorage and sessionStorage
  // should be null. These accesses shouldn't lead to a renderer kill.
  EXPECT_EQ(nullptr, content::EvalJs(pdf_frame, "window.localStorage"));
  EXPECT_EQ(nullptr, content::EvalJs(pdf_frame, "window.sessionStorage"));
  EXPECT_TRUE(pdf_frame->IsRenderFrameLive());
}
#endif  // BUILDFLAG(ENABLE_PDF)

// A helper class to verify that a "mailto:" external protocol request succeeds.
class MailtoExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  bool has_triggered_external_protocol() {
    return has_triggered_external_protocol_;
  }

  const GURL& external_protocol_url() { return external_protocol_url_; }

  content::WebContents* web_contents() { return web_contents_; }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {}

  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return new shell_integration::DefaultSchemeClientWorker(url);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::DONT_BLOCK;
  }

  void BlockRequest() override {}

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    external_protocol_url_ = url;
    web_contents_ = web_contents;
    has_triggered_external_protocol_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  void Wait() {
    if (!has_triggered_external_protocol_) {
      message_loop_runner_ = new content::MessageLoopRunner();
      message_loop_runner_->Run();
    }
  }

  void FinishedProcessingCheck() override {}

 private:
  bool has_triggered_external_protocol_ = false;
  GURL external_protocol_url_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// This test is not run on ChromeOS because it registers a custom handler (see
// ProtocolHandlerRegistry::InstallDefaultsForChromeOS), and handles mailto:
// navigations before getting to external protocol code.

// This test verifies that external protocol requests succeed when made from an
// OOPIF (https://crbug.com/668289).

// Disabled due to flakiness. If enabled, still skip for ChromeOS based on
// comment above.
// See https://crbug.com/980446
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       DISABLED_LaunchExternalProtocolFromSubframe) {
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  // Navigate to a page with a cross-site iframe that triggers a mailto:
  // external protocol request.
  // The test did not start by navigating to this URL because that would mask
  // the bug.  Instead, navigating the main frame to another page will cause a
  // cross-process transfer, which will avoid a situation where the OOPIF's
  // swapped-out RenderViewHost and the main frame's active RenderViewHost get
  // the same routing IDs, causing an accidental success.
  GURL mailto_main_frame_url(
      embedded_test_server()->GetURL("b.com", "/iframe.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), mailto_main_frame_url));

  MailtoExternalProtocolHandlerDelegate delegate;
  ExternalProtocolHandler::SetDelegateForTesting(&delegate);

  GURL mailto_subframe_url(
      embedded_test_server()->GetURL("c.com", "/page_with_mailto.html"));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      NavigateIframeToURL(active_web_contents, "test", mailto_subframe_url));

  delegate.Wait();

  EXPECT_TRUE(delegate.has_triggered_external_protocol());
  EXPECT_EQ(delegate.external_protocol_url(), GURL("mailto:mail@example.org"));
  EXPECT_EQ(active_web_contents, delegate.web_contents());
  ExternalProtocolHandler::SetDelegateForTesting(nullptr);
}

// Verify that a popup can be opened after navigating a remote frame.  This has
// to be a chrome/ test to ensure that the popup blocker doesn't block the
// popup.  See https://crbug.com/670770.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       NavigateRemoteFrameAndOpenPopup) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the iframe cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Run a script in the parent frame to (1) navigate iframe to another URL,
  // and (2) open a popup.  Note that ExecJs will run this with a user
  // gesture, so both steps should succeed.
  frame_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  const char kScriptTemplate[] = R"(
      document.querySelector('iframe').src = $1;
      !!window.open('about:blank'); )";
  bool popup_handle_is_valid =
      content::EvalJs(active_web_contents,
                      content::JsReplace(kScriptTemplate, frame_url))
          .ExtractBool();
  popup_observer.Wait();

  // The popup shouldn't be blocked.
  EXPECT_TRUE(popup_handle_is_valid);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

// Ensure that a transferred cross-process navigation does not generate
// DidStopLoading events until the navigation commits.  If it did, then
// ui_test_utils::NavigateToURL would proceed before the URL had committed.
// http://crbug.com/243957.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       NoStopDuringTransferUntilCommit) {
  GURL init_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), init_url));

  // Navigate to a same-site page that redirects, causing a transfer.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a RedirectObserver that goes away before we close the tab.
  {
    RedirectObserver redirect_observer(contents);
    GURL dest_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
    GURL redirect_url(embedded_test_server()->GetURL(
        "c.com", "/server-redirect?" + dest_url.spec()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));

    // We should immediately see the new committed entry.
    EXPECT_FALSE(contents->GetController().GetPendingEntry());
    EXPECT_EQ(dest_url,
              contents->GetController().GetLastCommittedEntry()->GetURL());

    // We should keep track of the original request URL, redirect chain, and
    // page transition type during a transfer, since these are necessary for
    // history autocomplete to work.
    EXPECT_EQ(redirect_url, contents->GetController()
                                .GetLastCommittedEntry()
                                ->GetOriginalRequestURL());
    EXPECT_EQ(2U, redirect_observer.redirects().size());
    EXPECT_EQ(redirect_url, redirect_observer.redirects().at(0));
    EXPECT_EQ(dest_url, redirect_observer.redirects().at(1));
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(redirect_observer.transition(),
                                             ui::PAGE_TRANSITION_TYPED));
  }
}

// Tests that a cross-process redirect will only cause the beforeunload
// handler to run once.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       SingleBeforeUnloadAfterRedirect) {
  // Navigate to a page with a beforeunload handler.
  GURL url(embedded_test_server()->GetURL("a.com", "/beforeunload.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to a URL that redirects to another process and approve the
  // beforeunload dialog that pops up.
  content::TestNavigationObserver nav_observer(contents);
  GURL dest_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "c.com", "/server-redirect?" + dest_url.spec()));
  browser()->OpenURL(content::OpenURLParams(redirect_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  javascript_dialogs::AppModalDialogController* alert =
      ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->is_before_unload_dialog());
  alert->view()->AcceptAppModalDialog();
  nav_observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PrintIgnoredInUnloadHandler) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(embedded_test_server()->GetURL("a.com", "/title1.html"))));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create 2 iframes and navigate them to b.com.
  EXPECT_TRUE(ExecJs(active_web_contents,
                     "var i = document.createElement('iframe'); i.id = "
                     "'child-0'; document.body.appendChild(i);"));
  EXPECT_TRUE(ExecJs(active_web_contents,
                     "var i = document.createElement('iframe'); i.id = "
                     "'child-1'; document.body.appendChild(i);"));
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-0",
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html"))));
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-1",
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html"))));

  content::RenderFrameHost* child_0 =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* child_1 =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 1);

  // Add an unload handler that calls print() to child-0 iframe.
  EXPECT_TRUE(
      ExecJs(child_0, "document.body.onunload = function() { print(); }"));

  // Transfer child-0 to a new process hosting c.com.
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-0",
      GURL(embedded_test_server()->GetURL("c.com", "/title1.html"))));

  // Check that b.com's process is still alive.
  EXPECT_EQ(true, EvalJs(child_1, "true;"));
}

// Ensure that when a window closes itself via window.close(), its process does
// not get destroyed if there's a pending cross-process navigation in the same
// process from another tab.  See https://crbug.com/799399.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       ClosePopupWithPendingNavigationInOpener) {
  // Start on a.com and open a popup to b.com.
  GURL opener_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecJs(opener_contents, "window.open('" + popup_url.spec() + "');"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(opener_contents, popup_contents);

  // This test technically performs a tab-under navigation. This will be blocked
  // if the tab-under blocking feature is enabled. Simulate clicking the opener
  // here to avoid that behavior.
  content::SimulateMouseClickAt(opener_contents, 0 /* modifiers */,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(50, 50));

  // From the popup, start a navigation in the opener to b.com, but don't
  // commit.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  content::TestNavigationManager manager(opener_contents, b_url);
  EXPECT_TRUE(
      ExecJs(popup_contents, "opener.location='" + b_url.spec() + "';"));
  // Since the pending RPH for b.com will be checked, we need to wait for it
  // to be created.
  manager.WaitForSpeculativeRenderFrameHostCreation();

  // Close the popup.  This should *not* kill the b.com process, as it still
  // has a pending navigation in the opener window.
  content::RenderProcessHost* b_com_rph =
      popup_contents->GetPrimaryMainFrame()->GetProcess();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecJs(popup_contents, "window.close();"));
  destroyed_watcher.Wait();
  EXPECT_TRUE(b_com_rph->IsInitializedAndNotDead());

  // Resume the pending navigation in the original tab and ensure it finishes
  // loading successfully.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(b_url,
            opener_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
}

#if defined(USE_AURA)
// Test that with a desktop/laptop touchscreen, a two finger tap opens a
// context menu in an OOPIF.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, TwoFingerTapContextMenu) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the iframe cross-site.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  content::RenderWidgetHostView* child_rwhv = child_frame->GetView();
  content::RenderWidgetHost* child_rwh = child_rwhv->GetRenderWidgetHost();

  ASSERT_TRUE(child_frame->IsCrossProcessSubframe());

  // Send a two finger tap event to the child and wait for the context menu to
  // open.
  ContextMenuWaiter menu_waiter;

  gfx::PointF child_location(1, 1);
  gfx::PointF child_location_in_root =
      child_rwhv->TransformPointToRootCoordSpaceF(child_location);

  blink::WebGestureEvent event(
      blink::WebInputEvent::Type::kGestureTwoFingerTap,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(child_location);
  event.SetPositionInScreen(child_location_in_root);
  event.data.two_finger_tap.first_finger_width = 10;
  event.data.two_finger_tap.first_finger_height = 10;

  child_rwh->ForwardGestureEvent(event);

  menu_waiter.WaitForMenuOpenAndClose();
}
#endif  // defined(USE_AURA)

// Check that cross-process postMessage preserves user gesture.  When a
// subframe with a user gesture postMessages its parent, the parent should be
// able to open a popup.  This test is in chrome/ so that it exercises the
// popup blocker logic.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       CrossProcessPostMessagePreservesUserGesture) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the iframe cross-site.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));

  // Add a postMessage handler in the top frame.  The handler opens a new
  // popup.
  GURL popup_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(ExecJs(
      web_contents,
      base::StringPrintf("window.addEventListener('message', function() {\n"
                         "  window.w = window.open('%s');\n"
                         "});",
                         popup_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Send a postMessage from the child frame to its parent.  Note that by
  // default ExecJs runs with a user gesture, which should be
  // transferred to the parent via postMessage. The parent should open the
  // popup in its message handler, and the popup shouldn't be blocked.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0),
                     "parent.postMessage('foo', '*')"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(popup_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Check that the window handle returned from window.open() was valid.
  EXPECT_EQ(true, content::EvalJs(web_contents, "!!window.w",
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Check that when a frame sends two cross-process postMessages while having a
// user gesture, the recipient will only be able to create one popup.  This
// test is in chrome/ so that it exercises the popup blocker logic.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       TwoPostMessagesWithSameUserGesture) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the iframe cross-site.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  // Add a postMessage handler in the root frame.  The handler opens a new popup
  // for a URL constructed using postMessage event data.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  EXPECT_TRUE(
      ExecJs(web_contents,
             base::StringPrintf(
                 "window.addEventListener('message', function(event) {\n"
                 "  window.w = window.open('%s' + event.data);\n"
                 "});",
                 popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Send two postMessages from child frame to parent frame as part of the same
  // user gesture.  Ensure that only one popup can be opened.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(child,
                     "parent.postMessage('title1.html', '*');\n"
                     "parent.postMessage('title2.html', '*');"));
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title1.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Ensure that only one popup can be opened.  The second window.open() call
  // should've failed and stored null into window.w.
  EXPECT_EQ(false, content::EvalJs(child, "!!window.w",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Check that when a frame sends two postMessages to iframes on different sites
// while having a user gesture, the two recipient processes will only be able
// to create one popup. This test is in chrome/ so that it exercises the popup
// blocker logic.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       TwoPostMessagesToDifferentSitesWithSameUserGesture) {
  // Start on a page a.com with two iframes on b.com and c.com.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_b =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* frame_c = ChildFrameAt(frame_b, 0);

  // Add a postMessage handler in root_frame and frame_b.  The handler opens a
  // new popup for a URL constructed using postMessage event data.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  const std::string script = base::StringPrintf(
      "window.addEventListener('message', function(event) {\n"
      "  window.w = window.open('%s' + event.data);\n"
      "});",
      popup_url.spec().c_str());
  EXPECT_TRUE(
      ExecJs(web_contents, script, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(ExecJs(frame_b, script, content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Add a popup observer.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();

  // Send two postMessages from the "leaf" frame to both its ancestors as part
  // of the same user gesture.
  EXPECT_TRUE(ExecJs(frame_c,
                     "parent.postMessage('title1.html', '*');\n"
                     "parent.parent.postMessage('title1.html', '*');"));

  // Ensure that only one popup can be opened.  Note that between the two OOPIF
  // processes, there is no ordering guarantee of which one will open the popup
  // first.
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title1.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Ensure that only one renderer process has a valid popup handle.
  bool root_frame_handle_is_valid =
      content::EvalJs(web_contents, "!!window.w",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();
  bool frame_b_handle_is_valid =
      content::EvalJs(frame_b, "!!window.w",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();
  EXPECT_TRUE(root_frame_handle_is_valid != frame_b_handle_is_valid)
      << "root_frame_handle_is_valid = " << root_frame_handle_is_valid
      << ", frame_b_handle_is_valid = " << frame_b_handle_is_valid;
}

// Check that when a frame sends a cross-process postMessage to a second frame
// while having a user gesture, and then the second frame sends another
// cross-process postMessage to a third frame, the second and third frames can
// only create one popup between the two of them. This test is in chrome/ so
// that it exercises the popup blocker logic.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       PostMessageSendsSecondPostMessageWithUserGesture) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* grandchild = ChildFrameAt(child, 0);
  ASSERT_TRUE(child->IsCrossProcessSubframe());
  ASSERT_TRUE(grandchild->IsCrossProcessSubframe());
  ASSERT_NE(child->GetProcess(),
            web_contents->GetPrimaryMainFrame()->GetProcess());
  ASSERT_NE(grandchild->GetProcess(), child->GetProcess());
  ASSERT_NE(grandchild->GetProcess(),
            web_contents->GetPrimaryMainFrame()->GetProcess());

  // Add a postMessage handler to middle frame to send another postMessage to
  // top frame and then immediately attempt window.open().
  GURL popup1_url(embedded_test_server()->GetURL("popup.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(
      child,
      base::StringPrintf("window.addEventListener('message', function() {\n"
                         "  parent.postMessage('foo', '*');\n"
                         "  window.w = window.open('%s');\n"
                         "});",
                         popup1_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Add a postMessage handler to top frame to attempt a window.open().
  GURL popup2_url(embedded_test_server()->GetURL("popup.com", "/title2.html"));
  EXPECT_TRUE(ExecJs(
      web_contents,
      base::StringPrintf("window.addEventListener('message', function() {\n"
                         "  window.w = window.open('%s');\n"
                         "});",
                         popup2_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Send a postMessage from bottom frame to middle frame as part of the same
  // user gesture.  Ensure that only one popup can be opened.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(grandchild, "parent.postMessage('foo', '*');"));
  popup_observer.Wait();

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(popup1_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Ensure that only one popup can be opened.  The second window.open() call at
  // top frame should fail, storing null into window.w.
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.w",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// Test that when a frame sends a cross-process postMessage and then requests a
// window.open(), and the message recipient also requests a window.open(), only
// one popup will be opened.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       PostMessageSenderAndReceiverRaceToCreatePopup) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the iframe cross-site.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  // Add a postMessage handler in the root frame.  The handler opens a new popup
  // for a URL constructed using postMessage event data.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(
      web_contents,
      base::StringPrintf("window.addEventListener('message', function() {\n"
                         "  window.w = window.open('%s');\n"
                         "});",
                         popup_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Send a postMessage from child frame to parent frame and then immediately
  // consume the user gesture with window.open().
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(
      child, base::StringPrintf(
                 "parent.postMessage('foo', '*');\n"
                 "window.setTimeout(\"window.w = window.open('%s')\", 0);",
                 popup_url.spec().c_str())));
  popup_observer.Wait();

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(popup_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Ensure that only one popup was opened, from either the parent or the child
  // frame, but not both.
  bool parent_popup_handle_is_valid =
      content::EvalJs(web_contents, "!!window.w",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();
  bool child_popup_handle_is_valid =
      content::EvalJs(child, "!!window.w",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();
  EXPECT_NE(parent_popup_handle_is_valid, child_popup_handle_is_valid)
      << " parent_popup_handle_is_valid=" << parent_popup_handle_is_valid
      << " child_popup_handle_is_valid=" << child_popup_handle_is_valid;

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

// Test that an activation is visible to the ancestors of the activated frame
// and not to the descendants.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       UserActivationVisibilityInAncestorFrame) {
  // Start on a page a.com with two iframes on b.com and c.com.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_b =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* frame_c = ChildFrameAt(frame_b, 0);

  // Activate frame_b by executing a dummy script.
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecJs(frame_b, no_op_script));

  // Add a popup observer.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();

  // Try opening popups from frame_c and root frame.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  EXPECT_TRUE(
      ExecJs(frame_c,
             base::StringPrintf("window.w = window.open('%s' + 'title1.html');",
                                popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(
      ExecJs(web_contents,
             base::StringPrintf("window.w = window.open('%s' + 'title2.html');",
                                popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait and check that only one popup has opened.
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title2.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Confirm that only the root_frame opened the popup.
  EXPECT_EQ(true, content::EvalJs(web_contents, "!!window.w;",
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(false, content::EvalJs(frame_c, "!!window.w",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test that when an activation is consumed, no frames in the frame tree can
// consume it again.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       UserActivationConsumptionAcrossFrames) {
  // Start on a page a.com with two iframes on b.com and c.com.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_b =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* frame_c =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 1);

  // Activate frame_b and frame_c by executing dummy scripts.
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecJs(frame_b, no_op_script));
  EXPECT_TRUE(ExecJs(frame_c, no_op_script));

  // Add a popup observer.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();

  // Try opening popups from all three frames.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  EXPECT_TRUE(
      ExecJs(frame_b,
             base::StringPrintf("window.w = window.open('%s' + 'title1.html');",
                                popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(
      ExecJs(frame_c,
             base::StringPrintf("window.w = window.open('%s' + 'title2.html');",
                                popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(
      ExecJs(web_contents,
             base::StringPrintf("window.w = window.open('%s' + 'title3.html');",
                                popup_url.spec().c_str()),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait and check that only one popup has opened.
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title1.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Confirm that only frame_b opened the popup.
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.w;",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(true, content::EvalJs(frame_b, "!!window.w;",
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(false, content::EvalJs(frame_c, "!!window.w;",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test that opening a window with `noopener` consumes user activation.
// crbug.com/1264543, crbug.com/1291210
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       UserActivationConsumptionNoopener) {
  // Start on a page a.com.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Activate the frame by executing a dummy script.
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecJs(web_contents, no_op_script));

  // Add a popup observer.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();

  // Open a popup from the frame, with `noopener`. This should consume
  // transient user activation.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  EXPECT_TRUE(ExecJs(
      web_contents,
      base::StringPrintf(
          "window.w = window.open('%s'+'title1.html', '_blank', 'noopener');",
          popup_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Try to open another popup.
  EXPECT_TRUE(ExecJs(
      web_contents,
      base::StringPrintf(
          "window.w = window.open('%s'+'title2.html', '_blank', 'noopener');",
          popup_url.spec().c_str()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait and check that only one popup was opened.
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title1.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);
}

// TODO(crbug.com/40106376): Flaky.
// Tests that a cross-site iframe runs its beforeunload handler when closing a
// tab.  See https://crbug.com/853021.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       DISABLED_TabCloseWithCrossSiteBeforeUnloadIframe) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* first_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Add a second tab and load a page with an iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* second_web_contents =
      tab_strip_model->GetActiveWebContents();
  EXPECT_NE(first_web_contents, second_web_contents);

  // Navigate iframe cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(second_web_contents, "test", frame_url));

  // Install a dialog-showing beforeunload handler in the iframe.
  content::RenderFrameHost* child =
      ChildFrameAt(second_web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(ExecJs(child, "window.onbeforeunload = () => { return 'x' };"));
  content::PrepContentsForBeforeUnloadTest(second_web_contents);

  // Close the second tab.  This should return false to indicate that we're
  // waiting for the beforeunload dialog.
  int previous_tab_count = tab_strip_model->count();
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(previous_tab_count, tab_strip_model->count());

  // Cancel the dialog and make sure the tab stays alive.
  auto* dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->CancelAppModalDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(second_web_contents, tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Try closing the tab again.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(previous_tab_count, tab_strip_model->count());

  // Accept the dialog and wait for tab close to complete.
  content::WebContentsDestroyedWatcher destroyed_watcher(second_web_contents);
  dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
  destroyed_watcher.Wait();
  EXPECT_EQ(first_web_contents, tab_strip_model->GetActiveWebContents());
}

// Tests that a same-site iframe runs its beforeunload handler when closing a
// tab.  Same as the test above, but for a same-site rather than cross-site
// iframe.  See https://crbug.com/1010456.
// Flaky (timeout) on Linux, ChromeOS, MacOS, and Windows (crbug.com/1033002)
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       DISABLED_TabCloseWithSameSiteBeforeUnloadIframe) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* first_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Add a second tab and load a page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* second_web_contents =
      tab_strip_model->GetActiveWebContents();
  EXPECT_NE(first_web_contents, second_web_contents);
  content::RenderFrameHost* child =
      ChildFrameAt(second_web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(child->GetSiteInstance(),
            second_web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Install a dialog-showing beforeunload handler in the iframe.
  EXPECT_TRUE(ExecJs(child, "window.onbeforeunload = () => { return 'x' };"));
  content::PrepContentsForBeforeUnloadTest(second_web_contents);

  // Close the second tab. This should return false to indicate that we're
  // waiting for the beforeunload dialog.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(2, tab_strip_model->count());

  // Cancel the dialog and make sure the tab stays alive.
  auto* dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->CancelAppModalDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(second_web_contents, tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Try closing the tab again.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(2, tab_strip_model->count());

  // Accept the dialog and wait for tab close to complete.
  content::WebContentsDestroyedWatcher destroyed_watcher(second_web_contents);
  dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
  destroyed_watcher.Wait();
  EXPECT_EQ(first_web_contents, tab_strip_model->GetActiveWebContents());
}

// Test that there's no crash in the following scenario:
// 1. a page opens a cross-site popup, where the popup has a cross-site iframe
//    with a beforeunload handler.
// 2. The user tries to close the popup, triggering beforeunload.
// 3. While waiting for the subframe's beforeunload, the original page closes
//    the popup via window.close().
// See https://crbug.com/866382.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       CrossProcessWindowCloseWithBeforeUnloadIframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* first_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Open a cross-site popup from the first page.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/iframe.html"));
  EXPECT_TRUE(ExecJs(first_web_contents,
                     base::StringPrintf("window.w = window.open('%s');",
                                        popup_url.spec().c_str())));
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* second_web_contents =
      tab_strip_model->GetActiveWebContents();
  EXPECT_NE(first_web_contents, second_web_contents);

  // Navigate popup iframe cross-site.
  GURL frame_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(second_web_contents, "test", frame_url));

  // Install a dialog-showing beforeunload handler in the iframe.
  content::RenderFrameHost* child =
      ChildFrameAt(second_web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(ExecJs(child, "window.onbeforeunload = () => { return 'x' };"));
  content::PrepContentsForBeforeUnloadTest(second_web_contents);

  // Close the second tab.  This should return false to indicate that we're
  // waiting for the beforeunload dialog.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(2, tab_strip_model->count());

  // From the first tab, execute window.close() on the popup and wait for the
  // second WebContents to be destroyed.
  content::WebContentsDestroyedWatcher destroyed_watcher(second_web_contents);
  EXPECT_TRUE(ExecJs(first_web_contents, "w.close()"));
  destroyed_watcher.Wait();
  EXPECT_EQ(first_web_contents, tab_strip_model->GetActiveWebContents());
}

// Test that there's no crash in the following scenario:
// 1. a page opens a cross-site popup, where the popup has two cross-site
//    iframes, with second having a beforeunload handler.
// 2. The user tries to close the popup, triggering beforeunload.
// 3. While waiting for second subframe's beforeunload, the original page
//    closes the popup via window.close().
// See https://crbug.com/866382.  This is a variant of the test above, but
// with two iframes, which used to trigger a different crash.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       CrossProcessWindowCloseWithTwoBeforeUnloadIframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* first_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Open a cross-site popup from the first page.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  GURL popup_url(
      embedded_test_server()->GetURL("b.com", "/two_iframes_blank.html"));
  EXPECT_TRUE(ExecJs(first_web_contents,
                     base::StringPrintf("window.w = window.open('%s');",
                                        popup_url.spec().c_str())));
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* second_web_contents =
      tab_strip_model->GetActiveWebContents();
  EXPECT_NE(first_web_contents, second_web_contents);

  // Navigate both popup iframes cross-site.
  GURL frame_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(second_web_contents, "iframe1", frame_url));
  EXPECT_TRUE(NavigateIframeToURL(second_web_contents, "iframe2", frame_url));

  // Install a dialog-showing beforeunload handler in the second iframe.
  content::RenderFrameHost* child =
      ChildFrameAt(second_web_contents->GetPrimaryMainFrame(), 1);
  EXPECT_TRUE(ExecJs(child, "window.onbeforeunload = () => { return 'x' };"));
  content::PrepContentsForBeforeUnloadTest(second_web_contents);

  // Close the second tab.  This should return false to indicate that we're
  // waiting for the beforeunload dialog.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(2, tab_strip_model->count());

  // From the first tab, execute window.close() on the popup and wait for the
  // second WebContents to be destroyed.
  content::WebContentsDestroyedWatcher destroyed_watcher(second_web_contents);
  EXPECT_TRUE(ExecJs(first_web_contents, "w.close()"));
  destroyed_watcher.Wait();
  EXPECT_EQ(first_web_contents, tab_strip_model->GetActiveWebContents());
}

class ChromeSitePerProcessTestWithVerifiedUserActivation
    : public ChromeSitePerProcessTest {
 public:
  ChromeSitePerProcessTestWithVerifiedUserActivation() {
    feature_list()->Reset();
    feature_list()->InitWithFeatures(
        /*enabled_features=*/{features::kBrowserVerifiedUserActivationMouse},
        // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/{features::kHttpsUpgrades});
  }
};

// Test mouse down activation notification with browser verification.
// TODO(crbug.com/40826005): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_UserActivationBrowserVerificationSameOriginSite \
  DISABLED_UserActivationBrowserVerificationSameOriginSite
#else
#define MAYBE_UserActivationBrowserVerificationSameOriginSite \
  UserActivationBrowserVerificationSameOriginSite
#endif
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTestWithVerifiedUserActivation,
                       MAYBE_UserActivationBrowserVerificationSameOriginSite) {
  // Start on a page a.com with same-origin iframe on a.com and cross-origin
  // iframe b.com.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_a =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* frame_b = ChildFrameAt(frame_a, 0);

  // The test becomes flaky if we don't wait for frame_a's hit-test data before
  // sending the mouse-event below (crbug.com/1119342).
  content::WaitForHitTestData(frame_a);

  // Activate frame_a by clicking at the midpoints of top-left corners of
  // frame_a and frame_b.
  gfx::Rect bounds_a = frame_a->GetView()->GetViewBounds();
  gfx::Rect bounds_b = frame_b->GetView()->GetViewBounds();
  content::SimulateMouseClickAt(web_contents, 0 /* modifiers */,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point((bounds_a.x() + bounds_b.x()) / 2,
                                           (bounds_a.y() + bounds_b.y()) / 2));

  // Add a popup observer.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();

  // Try opening popups from frame_b and root frame.
  GURL popup_url(embedded_test_server()->GetURL("popup.com", "/"));
  EXPECT_TRUE(
      ExecJs(frame_b,
             content::JsReplace("window.w = window.open($1 + 'title1.html');",
                                popup_url),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace("window.w = window.open($1 + 'title2.html');",
                                popup_url),
             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait and check that only one popup has opened.
  popup_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(embedded_test_server()->GetURL("popup.com", "/title2.html"),
            popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Confirm that only the root_frame opened the popup.
  EXPECT_EQ(true, content::EvalJs(web_contents, "!!window.w"));

  EXPECT_EQ(false, content::EvalJs(frame_b, "!!window.w"));
}

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, JSPrintDuringSwap) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHostWatcher watcher(
      contents->GetPrimaryMainFrame()->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // This file will attempt a cross-process navigation.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/print_during_load_with_broken_pdf_then_navigate.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Ensure the first process did not crash when the queued print() fires
  // during frame detach.
  EXPECT_TRUE(WaitForLoadStop(contents));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This test verifies that an OOPIF created in a tab on a secondary display
// doesn't initialize its device scale factor based on the primary display.
// Note: This test could probably be expanded to run on all ASH platforms.
// Disabled due to flakiness. https://crbug.com/1359423
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       DISABLED_TestInitialDSFForOOPIF) {
  // Spec for a two-display system, where the primary display has non-unit
  // device scale factor, but the secondary has unit device scale factor.
  // Note: this test could really work with any two scale factors, so long as
  // they're different.
  std::string display_spec("0+0-500x500*2,0+501-500x500");
  ash::ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay(display_spec);
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::GetScreen();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);
  Browser* browser_on_secondary_display = CreateBrowser(browser()->profile());

  // Open a page with an OOPIF on the secondary display.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  content::ProxyDSFObserver observer;
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser_on_secondary_display, main_url));
  observer.WaitForOneProxyHostCreation();

  EXPECT_EQ(1u, observer.num_creations());
  // If the OOPIF correctly gets its device_scale_factor from the secondary
  // screen, then it will satisfy the following expectation.
  EXPECT_EQ(1.f, observer.get_proxy_host_dsf(0));
}
#endif
