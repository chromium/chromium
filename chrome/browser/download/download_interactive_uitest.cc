// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

using content::DownloadManager;
using content::URLLoaderMonitor;
using content::WebContents;
using download::DownloadItem;

namespace {

class FencedFrameDownloadTest : public DownloadTestBase {
 public:
  FencedFrameDownloadTest() = default;
  ~FencedFrameDownloadTest() override = default;

  FencedFrameDownloadTest(const FencedFrameDownloadTest&) = delete;
  FencedFrameDownloadTest& operator=(const FencedFrameDownloadTest&) = delete;

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();

    // Add content/test/data for cross_site_iframe_factory.html.
    https_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  }

  WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// This test verifies when fenced frame untrusted network is disabled
// immediately after user right clicks and selects "Save Image As...", the
// download request is interrupted.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       NetworkCutoffInterruptSaveImageAs) {
  // Disable SafeBrowsing for testing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  // Sanity check that there is no downloads at the start of the test.
  ASSERT_TRUE(VerifyNoDownloads());

  // Navigate the fenced frame to a page with an image element.
  GURL fenced_frame_url(
      https_test_server()->GetURL("a.test", "/test_visual.html"));

  GURL main_url(https_test_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          GetWebContents()->GetPrimaryMainFrame());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Upon context menu shown, disable fenced frame untrusted network access.
  // Then execute "Save Image As...".
  ContextMenuWaiter context_menu_waiter(
      IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
      base::BindLambdaForTesting([fenced_frame_rfh]() {
        ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
            (async () => {
              return window.fence.disableUntrustedNetwork();
            })();
          )"));
      }));

  // Create observer for the download.
  auto download_observer =
      std::make_unique<content::DownloadTestObserverInterrupted>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  // Click inside the fenced frame.
  const gfx::PointF image_element(15, 15);

  // Right-click on the image element to open the context menu.
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, image_element);

  // Wait for the context menu to be shown.
  context_menu_waiter.WaitForMenuOpenAndClose();

  // The download request should be interrupted.
  download_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(download_items.size(), 1UL);
  ASSERT_EQ(download_items[0]->GetState(), DownloadItem::INTERRUPTED);
  EXPECT_EQ(download_items[0]->GetLastReason(),
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
}

// This test verifies when fenced frame untrusted network is disabled
// immediately after user right clicks and before "Save Audio As..." is
// selected, the download request is interrupted.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       NetworkCutoffInterruptSaveAudioAs) {
  // Disable SafeBrowsing for testing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  // Sanity check that there is no downloads at the start of the test.
  ASSERT_TRUE(VerifyNoDownloads());

  // Navigate the fenced frame to a page with an audio element.
  GURL fenced_frame_url(
      https_test_server()->GetURL("a.test", "/accessibility/html/audio.html"));

  GURL main_url(https_test_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          GetWebContents()->GetPrimaryMainFrame());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Upon context menu shown, disable fenced frame untrusted network access.
  // Then execute "Save Audio As...".
  ContextMenuWaiter context_menu_waiter(
      IDC_CONTENT_CONTEXT_SAVEAVAS,
      base::BindLambdaForTesting([fenced_frame_rfh]() {
        ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
            (async () => {
              return window.fence.disableUntrustedNetwork();
            })();
          )"));
      }));

  // Create observer for the download.
  auto download_observer =
      std::make_unique<content::DownloadTestObserverInterrupted>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  // Click inside the fenced frame.
  const gfx::PointF audio_element(15, 15);

  // Right-click on the audio element to open the context menu.
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for the context menu to be shown.
  context_menu_waiter.WaitForMenuOpenAndClose();

  // The download request should be interrupted.
  download_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(download_items.size(), 1UL);
  ASSERT_EQ(download_items[0]->GetState(), DownloadItem::INTERRUPTED);
  EXPECT_EQ(download_items[0]->GetLastReason(),
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
}

// This test verifies when fenced frame untrusted network is disabled
// immediately after user right clicks and before "Save Video As..." is
// selected, the download request is interrupted.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       NetworkCutoffInterruptSaveVideoAs) {
  // Disable SafeBrowsing for testing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  // Sanity check that there is no downloads at the start of the test.
  ASSERT_TRUE(VerifyNoDownloads());

  // Navigate the fenced frame to a page with a video element.
  GURL fenced_frame_url(https_test_server()->GetURL(
      "a.test", "/media/video-player-autoplay.html"));

  GURL main_url(https_test_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          GetWebContents()->GetPrimaryMainFrame());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Upon context menu shown, disable fenced frame untrusted network access.
  // Then execute "Save Video As...".
  ContextMenuWaiter context_menu_waiter(
      IDC_CONTENT_CONTEXT_SAVEAVAS,
      base::BindLambdaForTesting([fenced_frame_rfh]() {
        ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
            (async () => {
              return window.fence.disableUntrustedNetwork();
            })();
          )"));
      }));

  // Create observer for the download.
  auto download_observer =
      std::make_unique<content::DownloadTestObserverInterrupted>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  // Click inside the fenced frame.
  const gfx::PointF video_element(15, 15);

  // Right-click on the video element to open the context menu.
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, video_element);

  // Wait for the context menu to be shown.
  context_menu_waiter.WaitForMenuOpenAndClose();

  // The download request should be interrupted.
  download_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(download_items.size(), 1UL);
  ASSERT_EQ(download_items[0]->GetState(), DownloadItem::INTERRUPTED);
  EXPECT_EQ(download_items[0]->GetLastReason(),
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
}

// This test verifies when fenced frame untrusted network is disabled during an
// in-progress download request, the download is interrupted.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       NetworkCutoffInterruptInProgressSaveLinkAsDownload) {
  // Disable SafeBrowsing for testing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  DownloadManager* manager = DownloadManagerForBrowser(browser());

  // Configure the test server to simulate a slow download.
  https_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  EXPECT_TRUE(https_test_server()->Start());

  EXPECT_EQ(0, manager->BlockingShutdownCount());
  EXPECT_EQ(0, manager->InProgressCount());

  EnableFileChooser(true);

  // Sanity check that there is no downloads at the start of the test.
  ASSERT_TRUE(VerifyNoDownloads());

  // Navigate the fenced frame to a page with an anchor element.
  GURL fenced_frame_url =
      https_test_server()->GetURL("a.test", "/download-anchor-slow.html");

  GURL main_url(https_test_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          GetWebContents()->GetPrimaryMainFrame());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Upon context menu shown, execute "Save Link As...".
  ContextMenuWaiter context_menu_waiter(IDC_CONTENT_CONTEXT_SAVELINKAS);

  // Create observer for the download.
  auto in_progress_download_observer =
      std::make_unique<content::DownloadTestObserverInProgress>(manager, 1);
  auto interrupt_download_observer =
      std::make_unique<content::DownloadTestObserverInterrupted>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  // Monitor requests to the anchor element href URL.
  GURL anchor_href =
      GURL(EvalJs(fenced_frame_rfh, "document.getElementById('anchor').href")
               .ExtractString());

  URLLoaderMonitor monitor({anchor_href});

  // Get the coordinate of the anchor element.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Right-click on the anchor element to open the context menu.
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for the context menu to be shown.
  context_menu_waiter.WaitForMenuOpenAndClose();

  // Expect the slow download is in progress.
  in_progress_download_observer->WaitForFinished();
  EXPECT_EQ(1u, in_progress_download_observer->NumDownloadsSeenInState(
                    DownloadItem::IN_PROGRESS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
            (async () => {
              return window.fence.disableUntrustedNetwork();
            })();
          )"));

  // The request is stopped with fenced frame network revocation error code.
  EXPECT_EQ(monitor.WaitForRequestCompletion(anchor_href).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);

  // The download request should be interrupted because of the network status
  // change.
  interrupt_download_observer->WaitForFinished();

  DownloadManager::DownloadVector download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(download_items.size(), 1UL);
  ASSERT_EQ(download_items[0]->GetState(), DownloadItem::INTERRUPTED);
  EXPECT_EQ(download_items[0]->GetLastReason(),
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
}

}  // namespace
