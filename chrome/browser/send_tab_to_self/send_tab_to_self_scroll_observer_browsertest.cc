// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"

#include <map>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace send_tab_to_self {

namespace {

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<send_tab_to_self::StubSendTabToSelfSyncService>();
}

class SendTabToSelfScrollObserverBrowserTest : public InProcessBrowserTest {
 public:
  SendTabToSelfScrollObserverBrowserTest() = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildStubSendTabToSelfSyncService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    model_fake_ =
        SendTabToSelfSyncServiceFactory::GetForProfile(browser()->GetProfile())
            ->GetSendTabToSelfModel();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    model_fake_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<SendTabToSelfModel> model_fake_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollObserverBrowserTest,
                       RecordsScrollVolumeWithRestoration) {
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  PageContext page_context;
  // This text fragment exists at the bottom of the page (top: 10000px).
  page_context.scroll_position.text_fragment =
      TextFragmentData("Some text", "", "", "");

  const SendTabToSelfEntry* entry = model_fake_->SendEntry(
      test_url, "title", "device", page_context, NavigationHistory(),
      base::DoNothing(), ShareEntryPoint::kShareSheet);

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  // Mimic the user opening the received tab.
  base::HistogramTester histogram_tester;
  OpenEntryInNewForegroundTab(browser()->GetProfile(), *entry,
                              ShareActivatedEntryPoint::kAutoOpened);

  navigation_observer.Wait();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(SendTabToSelfScrollObserver::FromWebContents(web_contents));

  // Verify that the programmatic scroll to 10000px is NOT counted yet.
  // Note: Metrics are recorded on tab close/nav, so we can't check yet.

  // Simulate a manual scroll of 100px.
  content::SimulateGestureScrollSequence(web_contents, gfx::Point(0, 0),
                                         gfx::Vector2dF(0, -100));

  // Close the tab to trigger metric recording.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(),
      TabCloseTypes::CLOSE_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.Scroll.Volume.WithRestoration", 100, 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.Scroll.Volume.WithoutRestoration", 0);
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollObserverBrowserTest,
                       RecordsScrollVolumeWithoutRestoration) {
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  // No scroll position in PageContext.
  PageContext page_context;

  const SendTabToSelfEntry* entry = model_fake_->SendEntry(
      test_url, "title", "device", page_context, NavigationHistory(),
      base::DoNothing(), ShareEntryPoint::kShareSheet);

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  // Mimic the user opening the received tab.
  base::HistogramTester histogram_tester;
  OpenEntryInNewForegroundTab(browser()->GetProfile(), *entry,
                              ShareActivatedEntryPoint::kAutoOpened);

  navigation_observer.Wait();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(SendTabToSelfScrollObserver::FromWebContents(web_contents));

  // Simulate a manual scroll of 50px.
  content::SimulateGestureScrollSequence(web_contents, gfx::Point(0, 0),
                                         gfx::Vector2dF(0, -50));

  // Close the tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(),
      TabCloseTypes::CLOSE_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.Scroll.Volume.WithoutRestoration", 50, 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.Scroll.Volume.WithRestoration", 0);
}

}  // namespace

}  // namespace send_tab_to_self
