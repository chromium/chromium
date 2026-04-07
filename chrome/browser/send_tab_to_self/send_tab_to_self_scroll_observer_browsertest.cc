// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"

#include <map>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace send_tab_to_self {

namespace {

void SimulateOpeningReceivedTab(Browser* browser,
                                const SendTabToSelfEntry& entry) {
  SendTabToSelfToolbarBubbleController* controller =
      SendTabToSelfToolbarBubbleController::From(browser);

  if (!controller->IsBubbleShowing()) {
    PinnedToolbarActions* pinned_controller =
        browser->browser_window_features()->pinned_toolbar_actions();
    pinned_controller->ShowActionEphemerallyInToolbar(kActionSendTabToSelf,
                                                      true);
    auto anchor = pinned_controller->GetBubbleAnchor(kActionSendTabToSelf);
    controller->ShowBubble(entry, anchor);
  }

  ASSERT_TRUE(controller->IsBubbleShowing());
  controller->bubble()->OpenInNewTab();
}

class FakeSendTabToSelfModel : public TestSendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override {
    auto it = entries_.find(guid);
    return it != entries_.end() ? it->second.get() : nullptr;
  }

  const SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      const std::string& device_id,
      const PageContext& context,
      NavigationHistory navigation_history) override {
    // For testing purposes, we hardcode the guid as "guid" so we can reference
    // it in the Navigate call.
    auto entry = std::make_unique<SendTabToSelfEntry>(
        "guid", url, title, base::Time::Now(), device_id, "cache_guid", context,
        std::move(navigation_history));
    const SendTabToSelfEntry* raw_ptr = entry.get();
    entries_[raw_ptr->GetGUID()] = std::move(entry);
    return raw_ptr;
  }

 private:
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>> entries_;
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override { return &model_fake_; }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }

  FakeSendTabToSelfModel& model_fake() { return model_fake_; }

 protected:
  FakeSendTabToSelfModel model_fake_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class SendTabToSelfScrollObserverBrowserTest : public InProcessBrowserTest {
 public:
  SendTabToSelfScrollObserverBrowserTest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfPropagateScrollPosition);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestSendTabToSelfSyncService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    model_fake_ = &(static_cast<TestSendTabToSelfSyncService*>(
                        SendTabToSelfSyncServiceFactory::GetForProfile(
                            browser()->profile()))
                        ->model_fake());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    model_fake_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<FakeSendTabToSelfModel> model_fake_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollObserverBrowserTest,
                       RecordsScrollVolumeWithRestoration) {
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  PageContext page_context;
  // This text fragment exists at the bottom of the page (top: 10000px).
  page_context.scroll_position.text_fragment =
      TextFragmentData("Some text", "", "", "");

  model_fake_->AddEntry(test_url, "title", "device", page_context,
                        NavigationHistory());

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  // Mimic the user opening the received tab.
  const SendTabToSelfEntry* entry = model_fake_->GetEntryByGUID("guid");
  base::HistogramTester histogram_tester;
  SimulateOpeningReceivedTab(browser(), *entry);

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

  model_fake_->AddEntry(test_url, "title", "device", page_context,
                        NavigationHistory());

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  // Mimic the user opening the received tab.
  const SendTabToSelfEntry* entry = model_fake_->GetEntryByGUID("guid");
  base::HistogramTester histogram_tester;
  SimulateOpeningReceivedTab(browser(), *entry);

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
