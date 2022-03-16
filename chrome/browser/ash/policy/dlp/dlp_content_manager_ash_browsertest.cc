// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <functional>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_warn_notifier.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
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
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenShareReported(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kScreenShareWarned(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kWarn);

constexpr char kScreenShareBlockedNotificationId[] = "screen_share_dlp_blocked";
constexpr char kScreenSharePausedNotificationId[] =
    "screen_share_dlp_paused-label";
constexpr char kScreenShareResumedNotificationId[] =
    "screen_share_dlp_resumed-label";

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kGoogleUrl[] = "https://google.com";
constexpr char kSrcPattern[] = "example.com";
constexpr char kLabel[] = "label";
const std::u16string kApplicationTitle = u"example.com";

content::MediaStreamRequest CreateMediaStreamRequest(
    content::WebContents* web_contents,
    std::string requested_video_device_id,
    blink::mojom::MediaStreamType video_type) {
  return content::MediaStreamRequest(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID(), /*page_request_id=*/0,
      GURL(kExampleUrl), /*user_gesture=*/false, blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_id=*/std::string(), requested_video_device_id,
      blink::mojom::MediaStreamType::NO_SERVICE, video_type,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false);
}

}  // namespace

// TODO(crbug.com/1262948): Enable and modify for lacros.
class DlpContentManagerAshBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerAshBrowserTest() = default;
  ~DlpContentManagerAshBrowserTest() override = default;

  MockDlpWarnNotifier* CreateAndSetMockDlpWarnNotifier(bool should_proceed) {
    std::unique_ptr<MockDlpWarnNotifier> mock_notifier =
        std::make_unique<MockDlpWarnNotifier>(should_proceed);
    MockDlpWarnNotifier* mock_notifier_ptr = mock_notifier.get();
    helper_->SetWarnNotifierForTesting(std::move(mock_notifier));
    return mock_notifier_ptr;
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetUpOnMainThread() override {
    // Instantiate |DlpContentManagerTestHelper| after main thread has been
    // set up cause |DlpReportingManager| needs a sequenced task runner handle
    // to set up the report queue.
    helper_ = std::make_unique<DlpContentManagerTestHelper>();
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
    for (size_t i = 0; i < count; ++i) {
      EXPECT_THAT(events_[i], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                                  kSrcPattern, restriction, level)));
    }
  }

  void StartDesktopScreenShare(
      content::WebContents* web_contents,
      blink::mojom::MediaStreamRequestResult expected_result) {
    const GURL origin(kExampleUrl);
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
        /*request_pan_tilt_zoom_permission=*/false);
    DesktopCaptureAccessHandler access_handler{
        std::make_unique<FakeDesktopMediaPickerFactory>()};

    base::test::TestFuture<
        std::reference_wrapper<const blink::MediaStreamDevices>,
        blink::mojom::MediaStreamRequestResult,
        std::unique_ptr<content::MediaStreamUI>>
        test_future;

    access_handler.HandleRequest(
        web_contents, request,
        test_future.GetCallback<const blink::MediaStreamDevices&,
                                blink::mojom::MediaStreamRequestResult,
                                std::unique_ptr<content::MediaStreamUI>>(),
        /*extension=*/nullptr);

    ASSERT_TRUE(test_future.Wait()) << "MediaResponseCallback timed out.";

    EXPECT_EQ(test_future.Get<1>(), expected_result);
  }

  void CheckScreenshotRestriction(ScreenshotArea area, bool expected_allowed) {
    base::RunLoop run_loop;
    static_cast<DlpContentManagerAsh*>(helper_->GetContentManager())
        ->CheckScreenshotRestriction(
            area, base::BindLambdaForTesting([&](bool allowed) {
              EXPECT_EQ(expected_allowed, allowed);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Checks that there is an expected number of blocked/not blocked and
  // warned/not warned data points. Number of not blocked and not warned data
  // points is the difference between |total_count| and |blocked_count| and
  // |warned_count| respectfully.
  void VerifyHistogramCounts(int blocked_count,
                             int warned_count,
                             int total_count,
                             std::string blocked_suffix,
                             std::string warned_suffix) {
    ASSERT_GE(total_count, warned_count + blocked_count);
    ASSERT_GE(blocked_count, 0);
    ASSERT_GE(warned_count, 0);
    histogram_tester_.ExpectBucketCount(
        GetDlpHistogramPrefix() + blocked_suffix, true, blocked_count);
    histogram_tester_.ExpectBucketCount(
        GetDlpHistogramPrefix() + blocked_suffix, false,
        total_count - blocked_count);
    histogram_tester_.ExpectBucketCount(GetDlpHistogramPrefix() + warned_suffix,
                                        true, warned_count);
    histogram_tester_.ExpectBucketCount(GetDlpHistogramPrefix() + warned_suffix,
                                        false, total_count - warned_count);
  }

 protected:
  std::unique_ptr<DlpContentManagerTestHelper> helper_;
  base::HistogramTester histogram_tester_;
  MockDlpRulesManager* mock_rules_manager_;
  std::vector<DlpPolicyEvent> events_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsRestricted) {
  SetupReporting();
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

  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 4);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotRestricted);
  CheckScreenshotRestriction(fullscreen, /*expected=*/false);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/false);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 3);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 5);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 3u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 4);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 8);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 4u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/false);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/false);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 9);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);

  helper_->DestroyWebContents(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 7);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 12);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 7u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsWarned) {
  SetupReporting();
  auto* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier(/*should_proceed=*/false);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(7);

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

  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotWarned);
  CheckScreenshotRestriction(fullscreen, /*expected=*/false);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/false);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 3u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 4u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/false);
  CheckScreenshotRestriction(window, /*expected=*/false);
  CheckScreenshotRestriction(partial_in, /*expected=*/false);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 7u);

  helper_->DestroyWebContents(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 7u);
}

// Calls to CheckScreenshotRestriction() should not be reported if allowed.
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest, ScreenshotsReported) {
  SetupReporting();
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

  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotReported);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(window, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  helper_->DestroyWebContents(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected=*/true);
  CheckScreenshotRestriction(partial_in, /*expected=*/true);
  CheckScreenshotRestriction(partial_out, /*expected=*/true);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 19);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);
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
  // Check that the warning is not shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

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
  // Check that the warning is still not shown: this is to ensure that
  // RunUntilIdle doesn't succeed just because it's waiting for the dialog to be
  // dismissed.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  browser2->window()->Close();
  histogram_tester_.ExpectTotalCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureWarnedAtEndAllowed) {
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
  helper_->ChangeConfidentiality(web_contents1, kScreenshotWarned);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));
  // Check that the warning is still not shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  // Check that capture was not requested to be stopped via callback.
  run_loop.RunUntilIdle();

  base::MockCallback<ash::OnCaptureModeDlpRestrictionChecked>
      on_dlp_checked_at_video_end_cb;
  EXPECT_CALL(on_dlp_checked_at_video_end_cb, Run(true)).Times(1);
  EXPECT_CALL(on_dlp_checked_at_video_end_cb, Run(false)).Times(0);
  EXPECT_FALSE(helper_->HasAnyContentCached());
  capture_mode_delegate->StopObservingRestrictedContent(
      on_dlp_checked_at_video_end_cb.Get());
  // Check that the warning is now shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  // Hit Enter to "Save anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_TRUE(helper_->HasContentCachedForRestriction(
      web_contents1, DlpRulesManager::Restriction::kScreenshot));

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 0);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureWarnedAtEndCancelled) {
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
  helper_->ChangeConfidentiality(web_contents1, kScreenshotWarned);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));
  // Check that the warning is still not shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  // Check that capture was not requested to be stopped via callback.
  run_loop.RunUntilIdle();

  base::MockCallback<ash::OnCaptureModeDlpRestrictionChecked>
      on_dlp_checked_at_video_end_cb;
  EXPECT_CALL(on_dlp_checked_at_video_end_cb, Run(true)).Times(0);
  EXPECT_CALL(on_dlp_checked_at_video_end_cb, Run(false)).Times(1);
  EXPECT_FALSE(helper_->HasAnyContentCached());
  capture_mode_delegate->StopObservingRestrictedContent(
      on_dlp_checked_at_video_end_cb.Get());
  // Check that the warning is now shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  // Hit Enter to "Cancel".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_FALSE(helper_->HasAnyContentCached());

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kVideoCaptureInterruptedUMA, true, 0);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareNotification) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);
  manager->OnScreenShareStarted(
      kLabel, {media_id}, kApplicationTitle, base::BindRepeating([]() {
        FAIL() << "Stop callback should not be called.";
      }),
      base::DoNothing(), base::DoNothing());

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

  manager->OnScreenShareStopped(kLabel, media_id);

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
                       ScreenShareDisabledOrWarned) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
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
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/0,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  manager->CheckScreenShareRestriction(media_id, u"example.com",
                                       base::DoNothing());
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  VerifyHistogramCounts(/*blocked_count=*/1, /*warned_count=*/0,
                        /*total_count=*/2,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  manager->CheckScreenShareRestriction(media_id, u"example.com",
                                       base::DoNothing());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenShare,
                  DlpRulesManager::Level::kWarn)));
  VerifyHistogramCounts(/*blocked_count=*/1, /*warned_count=*/1,
                        /*total_count=*/3,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       CheckRunningScreenSharesIgnoredIfStillRestricted) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
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

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                /*stop_callback=*/base::DoNothing(),
                                /*state_change_callback=*/base::DoNothing(),
                                /*source_callback=*/base::DoNothing());

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);
  // Add an additional restriction check to mimic the situations in which one
  // user action causes multiple checks of the running screen shares.
  helper_->CheckRunningScreenShares();
  // Since there's no change in the level or the contents there should be only
  // one reporting event and UMA datapoint.
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       CheckRunningScreenSharesIgnoredIfStillWarned) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                /*stop_callback=*/base::DoNothing(),
                                /*state_change_callback=*/base::DoNothing(),
                                /*source_callback=*/base::DoNothing());

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  // Add an additional restriction check to mimic the situations in which one
  // user action causes multiple checks of the running screen shares.
  helper_->CheckRunningScreenShares();
  // Since there's no change in the level or the contents there should be only
  // one reporting event and one warning created.
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenSharePausedWhenConfidentialTabMoved) {
  SetupReporting();
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GURL(kExampleUrl)));
  aura::Window* browser1_window = browser()->window()->GetNativeWindow();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL(kGoogleUrl)));
  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();

  // Make second window content as confidential.
  helper_->ChangeConfidentiality(web_contents2, kScreenShareRestricted);

  // Resize both contents to be visible so that visibility state won't change.
  browser1->window()->SetBounds(gfx::Rect(0, 00, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(150, 150, 500, 500));

  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;
  // Explicitly specify that the stop callback should never be invoked.
  EXPECT_CALL(stop_cb, Run()).Times(0);
  testing::InSequence s;
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);

  // Start screen share of the first window.
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW, browser1_window);
  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb.Get(), state_change_cb.Get(),
                                /*source_callback=*/base::DoNothing());

  // Move restricted tab from second window to shared first window.
  std::unique_ptr<content::WebContents> moved_web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  browser1->tab_strip_model()->InsertWebContentsAt(
      0, std::move(moved_web_contents), TabStripModel::ADD_NONE);
  browser1->tab_strip_model()->ActivateTabAt(0);

  // Cleanup and check reporting.
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kScreenSharePausedOrResumedUMA, true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareWarnedDuringAllowed) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;
  // Explicitly specify that the stop callback should never be invoked.
  EXPECT_CALL(stop_cb, Run()).Times(0);
  testing::InSequence s;
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb.Get(), state_change_cb.Get(),
                                /*source_callback=*/base::DoNothing());
  // Nothing is emitted yet since there's "no change" in the restrictions -
  // normally there would be a metric logged in CheckScreenShareRestricted()
  // that's not called in this test.

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/1,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  // Hit Enter to "Share anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenShare)));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, false, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, true, 1);

  EXPECT_TRUE(helper_->HasContentCachedForRestriction(
      web_contents, DlpRulesManager::Restriction::kScreenShare));
  // The contents should already be cached as allowed by the user, so this
  // should not trigger a new warning. We have to switch the level in order to
  // trigger a proper check of restrictions, otherwise it would be skipped
  // altogether.
  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/2,
                        /*total_count=*/3,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnSilentProceededUMA, true,
      1);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareWarnedDuringCanceled) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;
  // Explicitly specify that the the screen share cannot be resumed.
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(0);

  testing::InSequence s;
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(stop_cb, Run()).Times(1);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb.Get(), state_change_cb.Get(),
                                /*source_callback=*/base::DoNothing());
  // Nothing is emitted yet since there's "no change" in the restrictions -
  // normally there would be a metric logged in CheckScreenShareRestricted()
  // that's not called in this test.

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/1,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  // Hit Esc to "Cancel".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_FALSE(helper_->HasAnyContentCached());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, false, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, true, 0);
  // The screen share should be stopped so would not be checked again, and this
  // should not trigger a new warning.
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/1,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       ScreenShareWarnedFromLacrosDuringAllowed) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;
  // Explicitly specify that the stop callback should never be invoked.
  EXPECT_CALL(stop_cb, Run()).Times(0);
  testing::InSequence s;
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb.Get(), state_change_cb.Get(),
                                /*source_callback=*/base::DoNothing());

  manager->OnWindowRestrictionChanged(browser()->window()->GetNativeWindow(),
                                      kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);

  // Hit Enter to "Share anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  // The window contents should already be cached as allowed by the user, so
  // this should not trigger a new warning.
  manager->OnWindowRestrictionChanged(browser()->window()->GetNativeWindow(),
                                      kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
}

class DlpContentManagerAshScreenShareBrowserTest
    : public DlpContentManagerAshBrowserTest {
 public:
  void StartDesktopScreenShare(
      content::WebContents* web_contents,
      blink::mojom::MediaStreamRequestResult expected_result) {
    const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                           content::DesktopMediaID::kFakeId);
    const std::string requested_video_device_id =
        content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
            web_contents->GetMainFrame()->GetProcess()->GetID(),
            web_contents->GetMainFrame()->GetRoutingID(),
            url::Origin::Create(GURL(kExampleUrl)), media_id,
            /*extension_name=*/"",
            content::DesktopStreamRegistryType::kRegistryStreamTypeDesktop);

    StartScreenShare(
        std::make_unique<DesktopCaptureAccessHandler>(
            std::make_unique<FakeDesktopMediaPickerFactory>()),
        web_contents,
        CreateMediaStreamRequest(
            web_contents, requested_video_device_id,
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE),
        expected_result, media_id);
  }

  void StartTabScreenShare(
      content::WebContents* web_contents,
      blink::mojom::MediaStreamRequestResult expected_result) {
    const content::DesktopMediaID media_id(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(
            web_contents->GetMainFrame()->GetProcess()->GetID(),
            web_contents->GetMainFrame()->GetRoutingID()));
    extensions::TabCaptureRegistry::Get(browser()->profile())
        ->AddRequest(web_contents, /*extension_id=*/"", /*is_anonymous=*/false,
                     GURL(kExampleUrl), media_id, /*extension_name=*/"",
                     web_contents);

    StartScreenShare(
        std::make_unique<TabCaptureAccessHandler>(), web_contents,
        CreateMediaStreamRequest(
            web_contents, /*requested_video_device_id=*/std::string(),
            blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE),
        expected_result, media_id);
  }

 private:
  void StartScreenShare(std::unique_ptr<MediaAccessHandler> handler,
                        content::WebContents* web_contents,
                        content::MediaStreamRequest request,
                        blink::mojom::MediaStreamRequestResult expected_result,
                        const content::DesktopMediaID& media_id) {
    // First check for the permission to start screen sharing.
    // It should call DlpContentManager::CheckScreenShareRestriction().
    base::test::TestFuture<
        std::reference_wrapper<const blink::MediaStreamDevices>,
        blink::mojom::MediaStreamRequestResult,
        std::unique_ptr<content::MediaStreamUI>>
        test_future;
    handler->HandleRequest(
        web_contents, request,
        test_future.GetCallback<const blink::MediaStreamDevices&,
                                blink::mojom::MediaStreamRequestResult,
                                std::unique_ptr<content::MediaStreamUI>>(),
        /*extension=*/nullptr);
    ASSERT_TRUE(test_future.Wait()) << "MediaResponseCallback timed out.";
    EXPECT_EQ(test_future.Get<1>(), expected_result);

    // Simulate starting screen sharing.
    // Calls DlpContentManager::OnScreenShareStarted().
    if (expected_result == blink::mojom::MediaStreamRequestResult::OK) {
      DlpContentManagerAsh* manager =
          static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
      EXPECT_CALL(stop_cb_, Run).Times(0);
      manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                    stop_cb_.Get(),
                                    /*state_change_callback*/ base::DoNothing(),
                                    /*source_callback=*/base::DoNothing());
    }
  }

  base::MockCallback<base::RepeatingClosure> stop_cb_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareRestricted) {
  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  StartDesktopScreenShare(
      web_contents, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  VerifyHistogramCounts(/*blocked_count=*/1, /*warned_count=*/0,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       TabScreenShareWarnedAllowed) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier(/*should_proceed=*/true);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(1);

  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  StartTabScreenShare(web_contents, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenShare,
                  DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenShare)));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  EXPECT_TRUE(helper_->HasContentCachedForRestriction(
      web_contents, DlpRulesManager::Restriction::kScreenShare));
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/2,
                        /*total_count=*/2,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, false, 0);

  helper_->ResetWarnNotifierForTesting();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       TabScreenShareWarnedCancelled) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier(/*should_proceed=*/false);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(1);

  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  StartTabScreenShare(
      web_contents, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  EXPECT_FALSE(helper_->HasAnyContentCached());
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/1,
                        /*total_count=*/1,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenShareWarnProceededUMA, false, 1);

  helper_->ResetWarnNotifierForTesting();
}

// Starting screen sharing and visiting other tabs should create exactly one
// reporting event.
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareReporting) {
  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenShareReported);

  StartTabScreenShare(web_contents, blink::mojom::MediaStreamRequestResult::OK);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kReport, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  VerifyHistogramCounts(/*blocked_count=*/0, /*warned_count=*/0,
                        /*total_count=*/2,
                        /*blocked_suffix=*/dlp::kScreenShareBlockedUMA,
                        /*warned_suffix=*/dlp::kScreenShareWarnedUMA);

  // Open new tab and navigate to a url.
  // Then move back to the screen-shared tab.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGoogleUrl)));
  ASSERT_NE(browser()->tab_strip_model()->GetActiveWebContents(), web_contents);
  ASSERT_EQ(web_contents->GetLastCommittedURL(), origin);
  ASSERT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            GURL(kGoogleUrl));
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

}  // namespace policy
