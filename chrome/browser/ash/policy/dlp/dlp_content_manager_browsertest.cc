// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/ash/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/ash/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/reporting/storage/test_storage_module.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace policy {

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenshotReported(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintAllowed(DlpContentRestriction::kPrint,
                                             DlpRulesManager::Level::kAllow);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintReported(DlpContentRestriction::kPrint,
                                              DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kVideoCaptureRestricted(
    DlpContentRestriction::kVideoCapture,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kVideoCaptureReported(
    DlpContentRestriction::kVideoCapture,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);

constexpr char kScreenCapturePausedNotificationId[] =
    "screen_capture_dlp_paused-label";
constexpr char kScreenCaptureResumedNotificationId[] =
    "screen_capture_dlp_resumed-label";
constexpr char kPrintBlockedNotificationId[] = "print_dlp_blocked";

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kUrl1[] = "https://example1.com";
constexpr char kUrl2[] = "https://example2.com";
constexpr char kUrl3[] = "https://example3.com";
constexpr char kUrl4[] = "https://example4.com";
constexpr char kSrcPattern[] = "example.com";
}  // namespace

class DlpContentManagerBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerBrowserTest() = default;
  ~DlpContentManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCaptureMode);
    InProcessBrowserTest::SetUp();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  // Sets up mock rules manager.
  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DlpContentManagerBrowserTest::SetDlpRulesManager,
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
    SetReportQueueForReportingManager(helper_.GetReportingManager(), events_);
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
  DlpContentManagerTestHelper helper_;
  base::HistogramTester histogram_tester_;
  MockDlpRulesManager* mock_rules_manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<DlpPolicyEvent> events_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsRestricted) {
  SetupReporting();
  DlpContentManager* manager = helper_.GetContentManager();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
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

  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 4);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  helper_.ChangeConfidentiality(web_contents, kScreenshotRestricted);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 3);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 5);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 3u);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 4);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 8);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 4u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 9);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);

  helper_.DestroyWebContents(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 12);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsReported) {
  SetupReporting();
  DlpContentManager* manager = helper_.GetContentManager();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
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

  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  helper_.ChangeConfidentiality(web_contents, kScreenshotReported);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 3u);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 4u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 7u);

  helper_.DestroyWebContents(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 19);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenConfidentialWindowResized) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_.ChangeConfidentiality(web_contents1, kVideoCaptureRestricted);

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

  capture_mode_delegate->StopObservingRestrictedContent();
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, VideoCaptureReported) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_.ChangeConfidentiality(web_contents1, kVideoCaptureReported);

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
  capture_mode_delegate->StopObservingRestrictedContent();

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenNonConfidentialWindowResized) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_.ChangeConfidentiality(web_contents1, kVideoCaptureRestricted);

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

  capture_mode_delegate->StopObservingRestrictedContent();
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureNotStoppedWhenConfidentialWindowHidden) {
  SetupReporting();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  helper_.ChangeConfidentiality(web_contents1, kVideoCaptureRestricted);

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
  capture_mode_delegate->StopObservingRestrictedContent();

  browser2->window()->Close();
  histogram_tester_.ExpectTotalCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       ScreenCaptureNotification) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManager* manager = helper_.GetContentManager();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);
  manager->OnScreenCaptureStarted("label", {media_id}, u"example.com",
                                  base::DoNothing());

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 0);

  helper_.ChangeConfidentiality(web_contents, kScreenShareRestricted);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 0);

  helper_.ChangeConfidentiality(web_contents, kEmptyRestrictionSet);

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 1);

  manager->OnScreenCaptureStopped("label", media_id);

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, PrintingNotRestricted) {
  // Set up mock report queue and mock rules manager.
  SetupReporting();
  ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  EXPECT_FALSE(helper_.GetContentManager()->IsPrintingRestricted(web_contents));

  // Start printing and check that there is no notification when printing is not
  // restricted.
  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*print_only_selection=*/false);
  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
  CheckEvents(DlpRulesManager::Restriction::kPrinting,
              DlpRulesManager::Level::kBlock, 0u);
}

class DlpContentManagerReportingBrowserTest
    : public DlpContentManagerBrowserTest {
 public:
  // Sets up real report queue together with TestStorageModule
  void SetupReportQueue() {
    const std::string dm_token_ = "FAKE_DM_TOKEN";
    const reporting::Destination destination_ =
        reporting::Destination::UPLOAD_EVENTS;

    storage_module_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();

    policy_check_callback_ =
        base::BindRepeating(&testing::MockFunction<reporting::Status()>::Call,
                            base::Unretained(&mocked_policy_check_));

    ON_CALL(mocked_policy_check_, Call())
        .WillByDefault(testing::Return(reporting::Status::StatusOK()));

    reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
        config_result = reporting::ReportQueueConfiguration::Create(
            dm_token_, destination_, policy_check_callback_);

    ASSERT_TRUE(config_result.ok());

    reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
        report_queue_result = reporting::ReportQueueImpl::Create(
            std::move(config_result.ValueOrDie()), storage_module_);

    ASSERT_TRUE(report_queue_result.ok());

    helper_.GetReportingManager()->GetReportQueueSetter().Run(
        std::move(report_queue_result.ValueOrDie()));
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
              CheckRecord(restriction, level, std::move(record));
              std::move(callback).Run(reporting::Status::StatusOK());
            })));
  }

 protected:
  scoped_refptr<reporting::StorageModuleInterface> storage_module_;
  testing::NiceMock<testing::MockFunction<reporting::Status()>>
      mocked_policy_check_;
  reporting::ReportQueueConfiguration::PolicyCheckCallback
      policy_check_callback_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingRestricted) {
  // Set up mock rules manager.
  SetupDlpRulesManager();
  // Set up real report queue.
  SetupReportQueue();
  // Sets an action to execute when an event arrives to a storage module.
  SetAddRecordCheck(DlpRulesManager::Restriction::kPrinting,
                    DlpRulesManager::Level::kBlock, /*times=*/2);

  ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  // No event should be emitted when there is no restriction.
  EXPECT_FALSE(helper_.GetContentManager()->IsPrintingRestricted(web_contents));

  // Set up printing restriction.
  helper_.ChangeConfidentiality(web_contents, kPrintRestricted);

  // Check that IsPrintingRestricted emitted an event.
  EXPECT_TRUE(helper_.GetContentManager()->IsPrintingRestricted(web_contents));

  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*print_only_selection=*/false);
  // Check for notification about printing restriction.
  EXPECT_TRUE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingReported) {
  SetupDlpRulesManager();
  SetupReportQueue();
  SetAddRecordCheck(DlpRulesManager::Restriction::kPrinting,
                    DlpRulesManager::Level::kReport, /*times=*/2);

  ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  // Set up printing restriction.
  helper_.ChangeConfidentiality(web_contents, kPrintReported);

  EXPECT_FALSE(helper_.GetContentManager()->IsPrintingRestricted(web_contents));

  printing::StartPrint(web_contents,
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
                       /*print_preview_disabled=*/false,
                       /*print_only_selection=*/false);
  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));

  // TODO(crbug/1213872, jkopanski): A hack to make this test working on
  // linux-chromeos-rel. For some reason, gtest calls RequestPrintPreview after
  // the test fixture, which triggers a DLP reporting event. This happens only
  // for linux-chromeos-rel build and in this PrintingReported test, does not
  // occur in PrintingRestricted test.
  helper_.ChangeConfidentiality(web_contents, kPrintAllowed);
}

class DlpContentManagerPolicyBrowserTest : public LoginPolicyTestBase {
 public:
  DlpContentManagerPolicyBrowserTest() = default;

  void SetDlpRulesPolicy(const base::Value& rules) {
    std::string json;
    base::JSONWriter::Write(rules, &json);

    base::DictionaryValue policy;
    policy.SetKey(key::kDataLeakPreventionRulesList, base::Value(json));
    user_policy_helper()->SetPolicyAndWait(
        policy, /*recommended=*/base::DictionaryValue(),
        ProfileManager::GetActiveUserProfile());
  }

 protected:
  DlpContentManagerTestHelper helper_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerPolicyBrowserTest,
                       GetRestrictionSetForURL) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kUrl1);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kUrl2);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrivacyScreenRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Block", std::move(src_urls2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  base::Value src_urls3(base::Value::Type::LIST);
  src_urls3.Append(kUrl3);
  base::Value restrictions3(base::Value::Type::LIST);
  restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrintingRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #3", "Block", std::move(src_urls3),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions3)));

  base::Value src_urls4(base::Value::Type::LIST);
  src_urls4.Append(kUrl4);
  base::Value restrictions4(base::Value::Type::LIST);
  restrictions4.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #4", "Block", std::move(src_urls4),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions4)));

  SetDlpRulesPolicy(rules);

  DlpContentRestrictionSet screenshot_and_videocapture(kScreenshotRestricted);
  screenshot_and_videocapture.SetRestriction(
      DlpContentRestriction::kVideoCapture, DlpRulesManager::Level::kBlock,
      GURL());
  EXPECT_EQ(screenshot_and_videocapture,
            helper_.GetRestrictionSetForURL(GURL(kUrl1)));
  EXPECT_EQ(kPrivacyScreenEnforced,
            helper_.GetRestrictionSetForURL(GURL(kUrl2)));
  EXPECT_EQ(kPrintRestricted, helper_.GetRestrictionSetForURL(GURL(kUrl3)));
  EXPECT_EQ(kScreenShareRestricted,
            helper_.GetRestrictionSetForURL(GURL(kUrl4)));
  EXPECT_EQ(DlpContentRestrictionSet(),
            helper_.GetRestrictionSetForURL(GURL(kExampleUrl)));
}

}  // namespace policy
