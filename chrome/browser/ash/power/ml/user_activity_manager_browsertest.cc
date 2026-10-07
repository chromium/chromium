// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/user_activity_manager.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash::power::ml {

namespace {

class TestingUserActivityUkmLogger : public UserActivityUkmLogger {
 public:
  TestingUserActivityUkmLogger() = default;
  TestingUserActivityUkmLogger(const TestingUserActivityUkmLogger&) = delete;
  TestingUserActivityUkmLogger& operator=(const TestingUserActivityUkmLogger&) =
      delete;
  ~TestingUserActivityUkmLogger() override = default;

  const std::vector<UserActivityEvent>& events() const { return events_; }

  // UserActivityUkmLogger:
  void LogActivity(const UserActivityEvent& event) override {
    events_.push_back(event);
  }

 private:
  std::vector<UserActivityEvent> events_;
};

}  // namespace

class UserActivityManagerBrowserTest : public InProcessBrowserTest {
 public:
  UserActivityManagerBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kUserActivityPrediction);
  }
  UserActivityManagerBrowserTest(const UserActivityManagerBrowserTest&) =
      delete;
  UserActivityManagerBrowserTest& operator=(
      const UserActivityManagerBrowserTest&) = delete;
  ~UserActivityManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
    activity_logger_ = std::make_unique<UserActivityManager>(
        &delegate_, ui::UserActivityDetector::Get(),
        chromeos::PowerManagerClient::Get(),
        session_manager::SessionManager::Get(),
        observer.InitWithNewPipeAndPassReceiver());
  }

  void TearDownOnMainThread() override {
    activity_logger_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void ReportUserActivity(const ui::Event* event) {
    activity_logger_->OnUserActivity(event);
  }

  void ReportIdleEvent(const IdleEventNotifier::ActivityData& data) {
    activity_logger_->UpdateAndGetSmartDimDecision(data, base::DoNothing());
  }

  ukm::SourceId NavigateInBrowser(
      BrowserWindowInterface* target_browser,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB) {
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        target_browser, url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabStripModel* const tab_strip_model = target_browser->GetTabStripModel();
    content::WebContents* const contents =
        disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB
            ? tab_strip_model->GetWebContentsAt(tab_strip_model->count() - 1)
            : tab_strip_model->GetActiveWebContents();
    return contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }

  TestingUserActivityUkmLogger delegate_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UserActivityManager> activity_logger_;
};

IN_PROC_BROWSER_TEST_F(UserActivityManagerBrowserTest, BasicTabs) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  const ukm::SourceId source_id1 =
      NavigateInBrowser(browser(), url1, WindowOpenDisposition::CURRENT_TAB);
  site_engagement::SiteEngagementService::Get(GetProfile())
      ->ResetBaseScoreForURL(url1, 95);
  NavigateInBrowser(browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB);

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(features.source_id(), source_id1);
  EXPECT_EQ(features.tab_domain(), url1.GetHost());
  EXPECT_FALSE(features.tab_domain().empty());
  EXPECT_EQ(features.engagement_score(), 90);
  EXPECT_FALSE(features.has_form_entry());
}

IN_PROC_BROWSER_TEST_F(UserActivityManagerBrowserTest, MultiBrowsersAndTabs) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 and browser3 are both visible but browser2 is the topmost.
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(GetProfile());
  BrowserWindowInterface* const browser3 = CreateBrowser(GetProfile());

  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");
  const GURL url4 = embedded_test_server()->GetURL("/title1.html?4");

  NavigateInBrowser(browser1, url1, WindowOpenDisposition::CURRENT_TAB);
  NavigateInBrowser(browser1, url2, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  const ukm::SourceId source_id3 =
      NavigateInBrowser(browser2, url3, WindowOpenDisposition::CURRENT_TAB);

  NavigateInBrowser(browser3, url4, WindowOpenDisposition::CURRENT_TAB);

  browser3->GetWindow()->Activate();
  browser2->GetWindow()->Activate();
  browser1->GetWindow()->Activate();
  browser1->GetWindow()->Minimize();
  browser2->GetWindow()->Activate();

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(features.source_id(), source_id3);
  EXPECT_EQ(features.tab_domain(), url3.GetHost());
  EXPECT_EQ(features.engagement_score(), 0);
  EXPECT_FALSE(features.has_form_entry());
}

IN_PROC_BROWSER_TEST_F(UserActivityManagerBrowserTest, Incognito) {
  BrowserWindowInterface* const incognito_browser =
      CreateIncognitoBrowser(GetProfile());

  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateInBrowser(incognito_browser, url1,
                    WindowOpenDisposition::CURRENT_TAB);
  NavigateInBrowser(incognito_browser, url2,
                    WindowOpenDisposition::NEW_BACKGROUND_TAB);
  incognito_browser->GetWindow()->Activate();

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_FALSE(features.has_source_id());
  EXPECT_FALSE(features.has_tab_domain());
  EXPECT_FALSE(features.has_engagement_score());
  EXPECT_FALSE(features.has_has_form_entry());
}

IN_PROC_BROWSER_TEST_F(UserActivityManagerBrowserTest, NoOpenTabs) {
  BrowserWindowInterface* const empty_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(GetProfile(),
                                /*from_user_gesture=*/false));
  empty_browser->GetWindow()->Show();
  empty_browser->GetWindow()->Activate();

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_FALSE(features.has_source_id());
  EXPECT_FALSE(features.has_tab_domain());
  EXPECT_FALSE(features.has_engagement_score());
  EXPECT_FALSE(features.has_has_form_entry());

  CloseBrowserSynchronously(empty_browser);
}

}  // namespace ash::power::ml
