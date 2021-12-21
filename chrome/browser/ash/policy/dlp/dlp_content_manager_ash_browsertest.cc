// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <functional>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/test_print_preview_dialog_cloned_observer.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/reporting/client/report_queue_impl.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace policy {

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenshotWarned(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenshotReported(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintAllowed(DlpContentRestriction::kPrint,
                                             DlpRulesManager::Level::kAllow);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintReported(DlpContentRestriction::kPrint,
                                              DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenShareReported(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kReport);

constexpr char kScreenShareBlockedNotificationId[] = "screen_share_dlp_blocked";
constexpr char kScreenSharePausedNotificationId[] =
    "screen_share_dlp_paused-label";
constexpr char kScreenShareResumedNotificationId[] =
    "screen_share_dlp_resumed-label";
constexpr char kPrintBlockedNotificationId[] = "print_dlp_blocked";

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kGoogleUrl[] = "https://google.com";
constexpr char kSrcPattern[] = "example.com";
}  // namespace

// TODO(crbug.com/1262948): Enable and modify for lacros.
class DlpContentManagerAshBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerAshBrowserTest() = default;
  ~DlpContentManagerAshBrowserTest() override = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetUpOnMainThread() override {
    // Instantiate |DlpContentManagerAshTestHelper| after main thread has been
    // set up cause |DlpReportingManager| needs a sequenced task runner handle
    // to set up the report queue.
    helper_ = std::make_unique<DlpContentManagerAshTestHelper>();
  }

  void TearDownOnMainThread() override { helper_.reset(); }

  // Sets up mock rules manager.
  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DlpContentManagerAshBrowserTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());

    EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
        .WillRepeatedly(testing::Return(kSrcPattern));
    EXPECT_CALL(*mock_rules_manager_, IsRestricted(_, _))
        .WillRepeatedly(testing::Return(DlpRulesManager::Level::kAllow));
  }

  void SetupReporting() {
    SetupDlpRulesManager();
    // Set up mock report queue.
    SetReportQueueForReportingManager(helper_->GetReportingManager(), events_,
                                      base::SequencedTaskRunnerHandle::Get());
  }

  void CheckEvents(DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level,
                   size_t count) {
    EXPECT_EQ(events_.size(), count);
    for (int i = 0; i < count; ++i) {
      EXPECT_THAT(events_[i], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                                  kSrcPattern, restriction, level)));
    }
  }

 protected:
  std::unique_ptr<DlpContentManagerAshTestHelper> helper_;
  base::HistogramTester histogram_tester_;
  MockDlpRulesManager* mock_rules_manager_;

 private:
  std::vector<DlpPolicyEvent> events_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsRestricted) {
  SetupReporting();
  DlpContentManagerAsh* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window =
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView());
  const gfx::Rect web_contents_rect = web_contents->GetContainerBounds();
  gfx::Rect out_rect(web_contents_rect);
  out_rect.Offset(web_contents_rect.width(), web_contents_rect.height());
  gfx::Rect in_rect(web_contents_rect);
  in_rect.Offset(web_contents_rect.width() / 2, web_contents_rect.height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 4);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotRestricted);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 3);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 5);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 3u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 4);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 8);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 4u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 9);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);

  helper_->DestroyWebContents(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 12);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsWarned) {
  SetupReporting();
  DlpContentManagerAsh* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window =
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView());
  const gfx::Rect web_contents_rect = web_contents->GetContainerBounds();
  gfx::Rect out_rect(web_contents_rect);
  out_rect.Offset(web_contents_rect.width(), web_contents_rect.height());
  gfx::Rect in_rect(web_contents_rect);
  in_rect.Offset(web_contents_rect.width() / 2, web_contents_rect.height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotWarned);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 3u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 4u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 7u);

  helper_->DestroyWebContents(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsReported) {
  SetupReporting();
  DlpContentManagerAsh* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window =
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView());
  const gfx::Rect web_contents_rect = web_contents->GetContainerBounds();
  gfx::Rect out_rect(web_contents_rect);
  out_rect.Offset(web_contents_rect.width(), web_contents_rect.height());
  gfx::Rect in_rect(web_contents_rect);
  in_rect.Offset(web_contents_rect.width() / 2, web_contents_rect.height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotReported);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 3u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 4u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 7u);

  helper_->DestroyWebContents(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 19);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureStoppedWhenConfidentialWindowResized) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GURL(kExampleUrl)));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL(kGoogleUrl)));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_->ChangeConfidentiality(web_contents1, kScreenshotRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), run_loop.QuitClosure());
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, VideoCaptureReported) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GURL(kExampleUrl)));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL(kGoogleUrl)));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_->ChangeConfidentiality(web_contents1, kScreenshotReported);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));

  // Check that capture was not requested to be stopped via callback.
  run_loop.RunUntilIdle();
  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureStoppedWhenNonConfidentialWindowResized) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GURL(kExampleUrl)));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL(kGoogleUrl)));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_->ChangeConfidentiality(web_contents1, kScreenshotRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), run_loop.QuitClosure());
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  // Move second window to make first window with confidential content visible.
  browser2->window()->SetBounds(gfx::Rect(150, 150, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureNotStoppedWhenConfidentialWindowHidden) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GURL(kExampleUrl)));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL(kGoogleUrl)));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_->ChangeConfidentiality(web_contents1, kScreenshotRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window, but keep confidential content hidden.
  browser1->window()->SetBounds(gfx::Rect(150, 150, 500, 500));

  // Check that capture was not requested to be stopped via callback.
  run_loop.RunUntilIdle();
  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());

  browser2->window()->Close();
  histogram_tester_.ExpectTotalCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareNotification) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);
  manager->OnScreenCaptureStarted("label", {media_id}, u"example.com",
                                  base::DoNothing());

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 0);

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 0);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 1);

  manager->OnScreenCaptureStopped("label", media_id);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareDisabledNotification) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);

  manager->CheckScreenShareRestriction(media_id, u"example.com",
                                       base::DoNothing());
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareBlockedUMA, false, 1);

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  manager->CheckScreenShareRestriction(media_id, u"example.com",
                                       base::DoNothing());
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareBlockedUMA, true, 1);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

// Starting screen sharing and navigating other tabs should create exactly one
// reporting event.
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenShareReporting) {
  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Setup screen sharing (media stream)
  auto picker_factory = std::make_unique<FakeDesktopMediaPickerFactory>();
  DesktopCaptureAccessHandler access_handler{std::move(picker_factory)};

  const std::string id =
      content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
          web_contents->GetMainFrame()->GetProcess()->GetID(),
          web_contents->GetMainFrame()->GetRoutingID(),
          url::Origin::Create(origin),
          content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                  content::DesktopMediaID::kFakeId),
          /*extension_name=*/"",
          content::DesktopStreamRegistryType::kRegistryStreamTypeDesktop);
  content::MediaStreamRequest request(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID(), /*page_request_id=*/0,
      origin, /*user_gesture=*/false, blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_id=*/std::string(), id,
      blink::mojom::MediaStreamType::NO_SERVICE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*region_capture_capable=*/false);

  base::test::TestFuture<
      std::reference_wrapper<const blink::MediaStreamDevices>,
      blink::mojom::MediaStreamRequestResult,
      std::unique_ptr<content::MediaStreamUI>>
      test_future;

  helper_->ChangeConfidentiality(web_contents, kScreenShareReported);

  access_handler.HandleRequest(
      web_contents, request,
      test_future.GetCallback<const blink::MediaStreamDevices&,
                              blink::mojom::MediaStreamRequestResult,
                              std::unique_ptr<content::MediaStreamUI>>(),
      /*extension=*/nullptr);

  ASSERT_TRUE(test_future.Wait()) << "Callback timed out.";

  EXPECT_EQ(test_future.Get<1>(), blink::mojom::MediaStreamRequestResult::OK);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kReport, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareBlockedUMA, false, 1);

  // Open new tab and navigate to a url.
  // Then move back to the screen-shared tab.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGoogleUrl)));
  ASSERT_NE(browser()->tab_strip_model()->GetActiveWebContents(), web_contents);
  // Just additional check that visiting a tab with restricted content does not
  // affect the shared tab.
  helper_->ChangeConfidentiality(
      browser()->tab_strip_model()->GetActiveWebContents(),
      kScreenShareRestricted);
  chrome::SelectNextTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), web_contents);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kReport, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, PrintingNotRestricted) {
  // Set up mock report queue and mock rules manager.
  SetupReporting();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  absl::optional<bool> is_printing_allowed;

  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents,
      base::BindOnce(
          [](absl::optional<bool>* out_result, bool should_proceed) {
            *out_result = absl::make_optional(should_proceed);
          },
          &is_printing_allowed));
  EXPECT_TRUE(is_printing_allowed);
  EXPECT_TRUE(is_printing_allowed.value());

  // Start printing and check that there is no notification when printing is not
  // restricted.
  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*has_selection=*/false);
  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
  CheckEvents(DlpRulesManager::Restriction::kPrinting,
              DlpRulesManager::Level::kBlock, 0u);
}

class DlpContentManagerReportingBrowserTest
    : public DlpContentManagerAshBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DlpContentManagerAshBrowserTest::SetUpOnMainThread();
    content::WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so |cloned_tab_observer_| can see it and create a
    // TestPrintViewManagerForRequestPreview for it before the real
    // PrintViewManager gets created.
    // Since TestPrintViewManagerForRequestPreview is created with
    // PrintViewManager::UserDataKey(), the real PrintViewManager is not created
    // and TestPrintViewManagerForRequestPreview gets mojo messages for the
    // purposes of this test.
    cloned_tab_observer_ =
        std::make_unique<printing::TestPrintPreviewDialogClonedObserver>(
            first_tab);
    chrome::DuplicateTab(browser());
  }

  void TearDownOnMainThread() override {
    DlpContentManagerAshBrowserTest::TearDownOnMainThread();
    cloned_tab_observer_.reset();
  }

  // Sets up real report queue together with TestStorageModule
  void SetupReportQueue() {
    const reporting::Destination destination_ =
        reporting::Destination::UPLOAD_EVENTS;

    storage_module_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();

    policy_check_callback_ =
        base::BindRepeating(&testing::MockFunction<reporting::Status()>::Call,
                            base::Unretained(&mocked_policy_check_));

    ON_CALL(mocked_policy_check_, Call())
        .WillByDefault(testing::Return(reporting::Status::StatusOK()));

    auto config_result = ::reporting::ReportQueueConfiguration::Create(
        ::reporting::EventType::kDevice, destination_, policy_check_callback_);

    ASSERT_TRUE(config_result.ok());

    // Create a report queue with the test storage module, and attach it
    // to an actual speculative report queue so we can override the one used in
    // |DlpReportingManager| by default.
    reporting::test::TestEvent<
        reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>>
        report_queue_event;
    reporting::ReportQueueImpl::Create(std::move(config_result.ValueOrDie()),
                                       storage_module_,
                                       report_queue_event.cb());
    auto report_queue_result = report_queue_event.result();

    ASSERT_TRUE(report_queue_result.ok());

    auto speculative_report_queue =
        ::reporting::SpeculativeReportQueueImpl::Create();
    auto attach_queue_cb =
        speculative_report_queue->PrepareToAttachActualQueue();

    helper_->GetReportingManager()->SetReportQueueForTest(
        std::move(speculative_report_queue));
    std::move(attach_queue_cb).Run(std::move(report_queue_result.ValueOrDie()));

    // Wait until the speculative report queue is initialized with the stubbed
    // report queue posted to its internal task runner
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  reporting::test::TestStorageModule* test_storage_module() const {
    reporting::test::TestStorageModule* test_storage_module =
        google::protobuf::down_cast<reporting::test::TestStorageModule*>(
            storage_module_.get());
    DCHECK(test_storage_module);
    return test_storage_module;
  }

  void CheckRecord(DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level,
                   reporting::Record record) {
    DlpPolicyEvent event;
    EXPECT_TRUE(event.ParseFromString(record.data()));
    EXPECT_EQ(event.source().url(), kSrcPattern);
    EXPECT_THAT(event, IsDlpPolicyEvent(CreateDlpPolicyEvent(
                           kSrcPattern, restriction, level)));
  }

  // Sets an action to execute when an event arrives to the report queue storage
  // module.
  void SetAddRecordCheck(DlpRulesManager::Restriction restriction,
                         DlpRulesManager::Level level,
                         int times) {
    // TODO(jkopanski): Change to [=, this] when chrome code base is updated to
    // C++20.
    EXPECT_CALL(*test_storage_module(), AddRecord)
        .Times(times)
        .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
            [=](reporting::Record record,
                base::OnceCallback<void(reporting::Status)> callback) {
              content::GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      &DlpContentManagerReportingBrowserTest::CheckRecord,
                      base::Unretained(this), restriction, level,
                      std::move(record)));
              std::move(callback).Run(reporting::Status::StatusOK());
            })));
  }

 protected:
  scoped_refptr<reporting::StorageModuleInterface> storage_module_;
  testing::NiceMock<testing::MockFunction<reporting::Status()>>
      mocked_policy_check_;
  reporting::ReportQueueConfiguration::PolicyCheckCallback
      policy_check_callback_;
  std::unique_ptr<printing::TestPrintPreviewDialogClonedObserver>
      cloned_tab_observer_;
};

// TODO(crbug.com/1265935): Add a test for warning
IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingRestricted) {
  // Set up mock rules manager.
  SetupDlpRulesManager();
  // Set up real report queue.
  SetupReportQueue();
  // Sets an action to execute when an event arrives to a storage module.
  SetAddRecordCheck(DlpRulesManager::Restriction::kPrinting,
                    DlpRulesManager::Level::kBlock, /*times=*/2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  absl::optional<bool> is_printing_allowed;
  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents,
      base::BindOnce(
          [](absl::optional<bool>* out_result, bool should_proceed) {
            *out_result = absl::make_optional(should_proceed);
          },
          &is_printing_allowed));
  EXPECT_TRUE(is_printing_allowed);
  EXPECT_TRUE(is_printing_allowed.value());

  // Set up printing restriction.
  helper_->ChangeConfidentiality(web_contents, kPrintRestricted);
  is_printing_allowed.reset();
  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents,
      base::BindOnce(
          [](absl::optional<bool>* out_result, bool should_proceed) {
            *out_result = absl::make_optional(should_proceed);
          },
          &is_printing_allowed));
  EXPECT_TRUE(is_printing_allowed);
  EXPECT_FALSE(is_printing_allowed.value());

  // Start printing and wait for the end of
  // printing::PrintViewManager::RequestPrintPreview(). StartPrint() is an
  // asynchronous function, which initializes mojo communication with a renderer
  // process. We need to wait for the DLP restriction check in
  // RequestPrintPreview(), which happens after the renderer process
  // communicates back to the browser process.
  base::RunLoop run_loop;
  printing::TestPrintViewManagerForRequestPreview::FromWebContents(web_contents)
      ->set_quit_closure(run_loop.QuitClosure());
  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*has_selection=*/false);
  run_loop.Run();

  // Check for notification about printing restriction.
  EXPECT_TRUE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
}

// For better understanding of this test see comments in
// DlpContentManagerReportingBrowserTest.PrintingRestricted test.
IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingReported) {
  SetupDlpRulesManager();
  SetupReportQueue();
  SetAddRecordCheck(DlpRulesManager::Restriction::kPrinting,
                    DlpRulesManager::Level::kReport, /*times=*/2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  // Set up printing restriction.
  absl::optional<bool> is_printing_allowed;
  helper_->ChangeConfidentiality(web_contents, kPrintReported);
  // Printing should be reported, but still allowed.
  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents,
      base::BindOnce(
          [](absl::optional<bool>* out_result, bool should_proceed) {
            *out_result = absl::make_optional(should_proceed);
          },
          &is_printing_allowed));
  EXPECT_TRUE(is_printing_allowed);
  EXPECT_TRUE(is_printing_allowed.value());

  base::RunLoop run_loop;
  printing::TestPrintViewManagerForRequestPreview::FromWebContents(web_contents)
      ->set_quit_closure(run_loop.QuitClosure());
  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*has_selection=*/false);
  run_loop.Run();

  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
}

}  // namespace policy
