// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_manager.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "chrome/browser/ash/power/ml/screen_brightness_event.pb.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash::power::ml {

namespace {

struct LogActivityInfo {
  ScreenBrightnessEvent screen_brightness_event;
  ukm::SourceId tab_id;
  bool has_form_entry;
};

class TestingAdaptiveScreenBrightnessUkmLogger
    : public AdaptiveScreenBrightnessUkmLogger {
 public:
  TestingAdaptiveScreenBrightnessUkmLogger() = default;
  TestingAdaptiveScreenBrightnessUkmLogger(
      const TestingAdaptiveScreenBrightnessUkmLogger&) = delete;
  TestingAdaptiveScreenBrightnessUkmLogger& operator=(
      const TestingAdaptiveScreenBrightnessUkmLogger&) = delete;
  ~TestingAdaptiveScreenBrightnessUkmLogger() override = default;

  const std::vector<LogActivityInfo>& log_activity_info() const {
    return log_activity_info_;
  }

  // AdaptiveScreenBrightnessUkmLogger:
  void LogActivity(const ScreenBrightnessEvent& screen_brightness_event,
                   ukm::SourceId tab_id,
                   bool has_form_entry) override {
    log_activity_info_.push_back(
        LogActivityInfo{screen_brightness_event, tab_id, has_form_entry});
  }

 private:
  std::vector<LogActivityInfo> log_activity_info_;
};

}  // namespace

class AdaptiveScreenBrightnessManagerBrowserTest : public InProcessBrowserTest {
 public:
  AdaptiveScreenBrightnessManagerBrowserTest() = default;
  AdaptiveScreenBrightnessManagerBrowserTest(
      const AdaptiveScreenBrightnessManagerBrowserTest&) = delete;
  AdaptiveScreenBrightnessManagerBrowserTest& operator=(
      const AdaptiveScreenBrightnessManagerBrowserTest&) = delete;
  ~AdaptiveScreenBrightnessManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    auto logger = std::make_unique<TestingAdaptiveScreenBrightnessUkmLogger>();
    ukm_logger_ = logger.get();

    mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
    auto periodic_timer = std::make_unique<base::RepeatingTimer>();
    screen_brightness_manager_ =
        std::make_unique<AdaptiveScreenBrightnessManager>(
            std::move(logger), ui::UserActivityDetector::Get(),
            chromeos::PowerManagerClient::Get(), nullptr, nullptr,
            observer.InitWithNewPipeAndPassReceiver(),
            std::move(periodic_timer));
  }

  void TearDownOnMainThread() override {
    ukm_logger_ = nullptr;
    screen_brightness_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestingAdaptiveScreenBrightnessUkmLogger* ukm_logger() { return ukm_logger_; }

  void FireTimer() { screen_brightness_manager_->OnTimerFired(); }

  void InitializeBrightness(const double level) {
    screen_brightness_manager_->OnReceiveScreenBrightnessPercent(level);
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

 private:
  std::unique_ptr<AdaptiveScreenBrightnessManager> screen_brightness_manager_;
  raw_ptr<TestingAdaptiveScreenBrightnessUkmLogger> ukm_logger_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AdaptiveScreenBrightnessManagerBrowserTest,
                       SingleBrowser) {
  NavigateInBrowser(browser(), embedded_test_server()->GetURL("/title1.html"),
                    WindowOpenDisposition::CURRENT_TAB);
  const ukm::SourceId source_id2 = NavigateInBrowser(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_FALSE(info[0].has_form_entry);
}

IN_PROC_BROWSER_TEST_F(AdaptiveScreenBrightnessManagerBrowserTest,
                       MultipleBrowsersWithActive) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized, so not visible.
  //  - browser2 and browser3 are both visible but browser2 is the topmost.
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(GetProfile());
  BrowserWindowInterface* const browser3 = CreateBrowser(GetProfile());

  NavigateInBrowser(browser1, embedded_test_server()->GetURL("/title1.html"));
  const ukm::SourceId source_id2 = NavigateInBrowser(
      browser2, embedded_test_server()->GetURL("/title2.html"));
  NavigateInBrowser(browser3, embedded_test_server()->GetURL("/title3.html"));

  browser3->GetWindow()->Activate();
  browser2->GetWindow()->Activate();
  browser1->GetWindow()->Activate();
  browser1->GetWindow()->Minimize();
  browser2->GetWindow()->Activate();

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_FALSE(info[0].has_form_entry);
}

IN_PROC_BROWSER_TEST_F(AdaptiveScreenBrightnessManagerBrowserTest,
                       MultipleBrowsersNoneActive) {
  // Simulates three browsers, none of which are active:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 and browser3 are both visible but not focused so not active.
  //  - browser2 is the topmost.
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(GetProfile());
  BrowserWindowInterface* const browser3 = CreateBrowser(GetProfile());

  NavigateInBrowser(browser1, embedded_test_server()->GetURL("/title1.html"));
  const ukm::SourceId source_id2 = NavigateInBrowser(
      browser2, embedded_test_server()->GetURL("/title2.html"));
  NavigateInBrowser(browser3, embedded_test_server()->GetURL("/title3.html"));

  browser3->GetWindow()->Activate();
  browser2->GetWindow()->Activate();
  browser1->GetWindow()->Activate();
  aura::client::GetFocusClient(browser1->GetWindow()->GetNativeWindow())
      ->FocusWindow(nullptr);
  browser1->GetWindow()->Minimize();
  EXPECT_FALSE(browser1->GetWindow()->IsVisible());
  EXPECT_FALSE(browser1->GetWindow()->IsActive());
  EXPECT_TRUE(browser2->GetWindow()->IsVisible());
  EXPECT_FALSE(browser2->GetWindow()->IsActive());
  EXPECT_TRUE(browser3->GetWindow()->IsVisible());
  EXPECT_FALSE(browser3->GetWindow()->IsActive());

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_FALSE(info[0].has_form_entry);
}

IN_PROC_BROWSER_TEST_F(AdaptiveScreenBrightnessManagerBrowserTest,
                       BrowsersWithIncognito) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 is visible but not focused so not active.
  //  - browser3 is visible and focused, but incognito.
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(GetProfile());
  BrowserWindowInterface* const browser3 = CreateIncognitoBrowser(GetProfile());

  NavigateInBrowser(browser1, embedded_test_server()->GetURL("/title1.html"));
  const ukm::SourceId source_id2 = NavigateInBrowser(
      browser2, embedded_test_server()->GetURL("/title2.html"));
  NavigateInBrowser(browser3, embedded_test_server()->GetURL("/title3.html"));

  browser2->GetWindow()->Activate();
  browser1->GetWindow()->Activate();
  browser1->GetWindow()->Minimize();
  browser3->GetWindow()->Activate();

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_FALSE(info[0].has_form_entry);
}

}  // namespace ash::power::ml
