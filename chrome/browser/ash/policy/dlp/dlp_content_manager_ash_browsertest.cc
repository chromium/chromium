// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/mock_dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"
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
constexpr char kChromeUrl[] = "https://chromium.org";
constexpr char kSrcPattern[] = "example.com";
constexpr char kRuleName[] = "rule #1";
constexpr char kRuleId[] = "testid1";
constexpr char kLabel[] = "label";
constexpr char kWindowId[] = "windowId123";
constexpr mojo::ReceiverId kReceiverId = 1;
const DlpRulesManager::RuleMetadata kRuleMetadata(kRuleName, kRuleId);
const std::u16string kApplicationTitle = u"example.com";

const base::TimeDelta kScreenShareResumeDelayForTesting = base::Milliseconds(0);

content::MediaStreamRequest CreateMediaStreamRequest(
    content::WebContents* web_contents,
    std::string requested_video_device_id,
    blink::mojom::MediaStreamType video_type) {
  return content::MediaStreamRequest(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
      web_contents->GetPrimaryMainFrame()->GetRoutingID(),
      /*page_request_id=*/0, url::Origin::Create(GURL(kExampleUrl)),
      /*user_gesture=*/false, blink::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_id=*/{}, {requested_video_device_id},
      blink::mojom::MediaStreamType::NO_SERVICE, video_type,
      /*disable_local_echo=*/false,
      /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
}

}  // namespace

// These tests are for Ash-only features.
// Other features should be tested in dlp_content_manager_browsertest.cc or
// dlp_content_manager_lacros_browsertest.cc.
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

  MockDlpWarnNotifier* CreateAndSetMockDlpWarnNotifier() {
    std::unique_ptr<MockDlpWarnNotifier> mock_notifier =
        std::make_unique<MockDlpWarnNotifier>();
    MockDlpWarnNotifier* mock_notifier_ptr = mock_notifier.get();
    helper_->SetWarnNotifierForTesting(std::move(mock_notifier));
    return mock_notifier_ptr;
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
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

    EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _, _))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<3>(kRuleMetadata),
                                       testing::Return(kSrcPattern)));
    EXPECT_CALL(*mock_rules_manager_, IsRestricted(_, _))
        .WillRepeatedly(testing::Return(DlpRulesManager::Level::kAllow));
    EXPECT_CALL(*mock_rules_manager_, GetReportingManager())
        .Times(testing::AnyNumber());
    ;
  }

  void SetupReporting() {
    SetupDlpRulesManager();
    // Set up mock report queue.
    SetReportQueueForReportingManager(
        helper_->GetReportingManager(), events_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void CheckEvents(DlpRulesManager::Restriction restriction,
                   const std::string& src_url,
                   DlpRulesManager::Level level,
                   size_t count) {
    EXPECT_EQ(events_.size(), count);
    for (size_t i = 0; i < count; ++i) {
      EXPECT_THAT(
          events_[i],
          data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
              src_url, restriction, kRuleName, kRuleId, level)));
    }
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
        data_controls::GetDlpHistogramPrefix() + blocked_suffix, true,
        blocked_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + blocked_suffix, false,
        total_count - blocked_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + warned_suffix, true,
        warned_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + warned_suffix, false,
        total_count - warned_count);
  }

 protected:
  std::unique_ptr<DlpContentManagerTestHelper> helper_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_;
  std::vector<DlpPolicyEvent> events_;
};

struct ScreenshotTestParams {
  ScreenshotTestParams(std::string test_name,
                       DlpRulesManager::Level level,
                       std::vector<int> blocked_counts,
                       std::vector<int> warned_counts,
                       std::vector<int> total_counts,
                       std::vector<size_t> report_event_counts,
                       bool expect_allowed,
                       int warning_dialog_count = 0)
      : test_name(std::move(test_name)),
        level(level),
        restriction_set(DlpContentRestriction::kScreenshot, level),
        warning_dialog_count(warning_dialog_count),
        blocked_counts(std::move(blocked_counts)),
        warned_counts(std::move(warned_counts)),
        total_counts(std::move(total_counts)),
        report_event_counts(std::move(report_event_counts)),
        expect_allowed(expect_allowed) {}

  ~ScreenshotTestParams() = default;

  std::string test_name;
  DlpRulesManager::Level level;
  DlpContentRestrictionSet restriction_set;
  // Total number of expected warning dialogs. Once bypassed, warning is not
  // shown for the same content.
  int warning_dialog_count;
  // Numbers of expected block, warn, and total UMA histogram points.
  std::vector<int> blocked_counts;
  std::vector<int> warned_counts;
  std::vector<int> total_counts;
  // Number of expected report events. Note that this can differ from
  // total_counts (UMA) as report events are deduplicated.
  std::vector<size_t> report_event_counts;
  bool expect_allowed;
};

class ScreenshotTest
    : public DlpContentManagerAshBrowserTest,
      public testing::WithParamInterface<ScreenshotTestParams> {
 public:
  ScreenshotTest() = default;
  ~ScreenshotTest() override = default;

 protected:
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
};

IN_PROC_BROWSER_TEST_P(ScreenshotTest, CheckRestriction) {
  const ScreenshotTestParams& param = GetParam();
  SetupReporting();
  auto* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier(/*should_proceed=*/param.expect_allowed);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog)
      .Times(param.warning_dialog_count);

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

  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(window, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      param.blocked_counts[0], param.warned_counts[0],
      /*total_count=*/4,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              web_contents->GetLastCommittedURL().spec(), param.level,
              param.report_event_counts[0]);

  helper_->ChangeConfidentiality(web_contents, param.restriction_set);
  CheckScreenshotRestriction(fullscreen,
                             /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(window, /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(partial_in,
                             /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      param.blocked_counts[1], param.warned_counts[1],
      /*total_count=*/8,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              web_contents->GetLastCommittedURL().spec(), param.level,
              param.report_event_counts[1]);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(window, /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      param.blocked_counts[2], param.warned_counts[2],
      /*total_count=*/12,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              web_contents->GetLastCommittedURL().spec(), param.level,
              param.report_event_counts[2]);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  CheckScreenshotRestriction(fullscreen,
                             /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(window, /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(partial_in,
                             /*expected_allowed=*/param.expect_allowed);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      param.blocked_counts[3], param.warned_counts[3],
      /*total_count=*/16,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              web_contents->GetLastCommittedURL().spec(), param.level,
              param.report_event_counts[3]);

  helper_->DestroyWebContents(web_contents);
  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      param.blocked_counts[3], param.warned_counts[3],
      /*total_count=*/19,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              web_contents->GetLastCommittedURL().spec(), param.level,
              param.report_event_counts[3]);
}

IN_PROC_BROWSER_TEST_F(ScreenshotTest, WarningProceededReportedAfterCapture) {
  SetupReporting();
  auto* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier(/*should_proceed=*/true);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenshotWarned);
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  web_contents->GetLastCommittedURL().spec(),
                  DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
                  DlpRulesManager::Level::kWarn)));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 0);
  static_cast<DlpContentManagerAsh*>(helper_->GetContentManager())
      ->OnImageCapture(fullscreen);
  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
          web_contents->GetLastCommittedURL().spec(),
          DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId)));
}

IN_PROC_BROWSER_TEST_F(ScreenshotTest, CheckRestriction_Blocked_Lacros) {
  SetupReporting();

  // Create a Lacros-like Exo surface.
  exo::WMHelper wm_helper;
  std::unique_ptr<exo::ShellSurface> shell_surface =
      exo::test::ShellSurfaceBuilder({640, 480}).BuildShellSurface();
  shell_surface->root_surface()->window()->TrackOcclusionState();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window_area = ScreenshotArea::CreateForWindow(window);
  const gfx::Rect rect = window->GetBoundsInRootWindow();
  gfx::Rect out_rect(rect);
  out_rect.Offset(window->GetBoundsInRootWindow().width(),
                  window->GetBoundsInRootWindow().height());
  gfx::Rect in_rect(rect);
  in_rect.Offset(window->GetBoundsInRootWindow().width() / 2,
                 window->GetBoundsInRootWindow().height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(window_area, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/4,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
              DlpRulesManager::Level::kBlock, 0u);

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  exo::SetShellApplicationId(window, kWindowId);
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenshotRestricted);
  CheckScreenshotRestriction(fullscreen, false);
  CheckScreenshotRestriction(window_area, false);
  CheckScreenshotRestriction(partial_in, false);
  CheckScreenshotRestriction(partial_out, true);
  VerifyHistogramCounts(
      /*blocked_count=*/3, /*warned_count=*/0,
      /*total_count=*/8,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
              DlpRulesManager::Level::kBlock, 3u);

  window->Hide();
  manager->OnWindowOcclusionChanged(window);
  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(window_area, /*expected_allowed=*/false);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      /*blocked_count=*/4, /*warned_count=*/0,
      /*total_count=*/12,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
              DlpRulesManager::Level::kBlock, 4u);

  window->Show();
  manager->OnWindowOcclusionChanged(window);
  CheckScreenshotRestriction(fullscreen, false);
  CheckScreenshotRestriction(window_area, false);
  CheckScreenshotRestriction(partial_in, false);
  CheckScreenshotRestriction(partial_out, true);
  VerifyHistogramCounts(
      /*blocked_count=*/7, /*warned_count=*/0,
      /*total_count=*/16,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
              DlpRulesManager::Level::kBlock, 7u);

  manager->OnWindowDestroying(window);
  CheckScreenshotRestriction(fullscreen, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_in, /*expected_allowed=*/true);
  CheckScreenshotRestriction(partial_out, /*expected_allowed=*/true);
  VerifyHistogramCounts(
      /*blocked_count=*/7, /*warned_count=*/0,
      /*total_count=*/19,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
              DlpRulesManager::Level::kBlock, 7u);
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
  ASSERT_EQ(events_.size(), 0u);

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      true, 1);
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  web_contents1->GetLastCommittedURL().spec(),
                  DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
                  DlpRulesManager::Level::kBlock)));
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
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      true, 0);
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  web_contents1->GetLastCommittedURL().spec(),
                  DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
                  DlpRulesManager::Level::kReport)));
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
  ASSERT_EQ(events_.size(), 0u);

  // Move second window to make first window with confidential content visible.
  browser2->window()->SetBounds(gfx::Rect(150, 150, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      true, 1);
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  web_contents1->GetLastCommittedURL().spec(),
                  DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
                  DlpRulesManager::Level::kBlock)));
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
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      0);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot, kSrcPattern,
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
  capture_mode_delegate->StopObservingRestrictedContent(
      on_dlp_checked_at_video_end_cb.Get());
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnedUMA,
      true, 1);
  // Check that the warning is now shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  // Hit Enter to "Save anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      true, 1);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      true, 0);
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
  capture_mode_delegate->StopObservingRestrictedContent(
      on_dlp_checked_at_video_end_cb.Get());
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnedUMA,
      true, 1);
  // Check that the warning is now shown.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  // Hit Enter to "Cancel".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 1);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  browser2->window()->Close();
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kVideoCaptureInterruptedUMA,
      true, 0);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshBrowserTest,
                       VideoCaptureWarningShowsLatestTitle) {
  SetupReporting();
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  testing::StrictMock<base::MockOnceClosure> stop_cb_;
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), stop_cb_.Get());

  helper_->ChangeConfidentiality(web_contents, kScreenshotWarned);
  // Check that the warning not shown yet, but the contents are already stored.
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  ASSERT_TRUE(helper_->GetRunningVideoCaptureInfo().has_value());
  auto actual_contents = helper_->GetRunningVideoCaptureInfo()
                             ->confidential_contents.GetContents();
  EXPECT_EQ(actual_contents.size(), 1u);
  EXPECT_EQ(actual_contents.begin()->title, u"example.com");

  // Change the title.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
            document.title = 'New Title';
  )"));

  DlpConfidentialContents expected_contents;
  expected_contents.Add(web_contents);
  EXPECT_CALL(*mock_dlp_warn_notifier,
              ShowDlpWarningDialog(
                  testing::_, DlpWarnDialog::DlpWarnDialogOptions(
                                  DlpWarnDialog::Restriction::kVideoCapture,
                                  expected_contents)))
      .Times(1);

  ASSERT_TRUE(helper_->GetRunningVideoCaptureInfo().has_value());
  actual_contents = helper_->GetRunningVideoCaptureInfo()
                        ->confidential_contents.GetContents();
  EXPECT_EQ(actual_contents.size(), 1u);
  EXPECT_EQ(actual_contents.begin()->title, u"New Title");

  run_loop.RunUntilIdle();
  capture_mode_delegate->StopObservingRestrictedContent(base::DoNothing());
}

class DlpContentManagerAshScreenShareBrowserTest
    : public DlpContentManagerAshBrowserTest {
 protected:
  // First checks whether screen sharing is allowed or not, and if yes,
  // simulates starting a full screen share.
  // Returns media_id created that can be used to identify the share.
  const content::DesktopMediaID MaybeStartFullScreenShare(
      content::WebContents* web_contents,
      bool expect_allowed = true,
      bool expect_warning = false) {
    const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                           content::DesktopMediaID::kFakeId);
    const std::string requested_video_device_id =
        content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
            web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
            web_contents->GetPrimaryMainFrame()->GetRoutingID(),
            url::Origin::Create(GURL(kExampleUrl)), media_id,
            content::DesktopStreamRegistryType::kRegistryStreamTypeDesktop);

    MaybeStartScreenShare(
        std::make_unique<DesktopCaptureAccessHandler>(
            std::make_unique<FakeDesktopMediaPickerFactory>()),
        web_contents,
        CreateMediaStreamRequest(
            web_contents, requested_video_device_id,
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE),
        media_id, expect_allowed, expect_warning);

    return media_id;
  }

  // First checks whether screen sharing is allowed or not, and if yes,
  // simulates starting a tab share.
  // Returns media_id created that can be used to identify the share.
  const content::DesktopMediaID MaybeStartTabShare(
      content::WebContents* web_contents,
      bool expect_allowed = true,
      bool expect_warning = false) {
    int process_id = web_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
    int frame_id = web_contents->GetPrimaryMainFrame()->GetRoutingID();
    const content::DesktopMediaID media_id(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(process_id, frame_id));

    extensions::TabCaptureRegistry::Get(browser()->profile())
        ->AddRequest(web_contents, /*extension_id=*/"", /*is_anonymous=*/false,
                     GURL(kExampleUrl), media_id, process_id, frame_id);

    MaybeStartScreenShare(
        std::make_unique<TabCaptureAccessHandler>(), web_contents,
        CreateMediaStreamRequest(
            web_contents, /*requested_video_device_id=*/std::string(),
            blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE),
        media_id, expect_allowed, expect_warning);

    return media_id;
  }

  // Stops the screen share for |media_id|.
  void StopScreenShare(const content::DesktopMediaID& media_id) {
    static_cast<DlpContentManagerAsh*>(helper_->GetContentManager())
        ->OnScreenShareStopped(kLabel, media_id);
  }

  // Asserts that there is an open warning dialog and sends a key press to
  // dimiss it and get expected result based on |allow|.
  void DismissDialog(bool allow) {
    ASSERT_EQ(helper_->ActiveWarningDialogsCount(), 1);
    ui::KeyboardCode key = allow ? ui::VKEY_RETURN : ui::VKEY_ESCAPE;
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), key, /*control=*/false,
        /*shift=*/false, /*alt=*/false, /*command=*/false));
  }

  // Blocks the test execution to wait for a screen share to be resumed.
  void WaitForScreenShareResume() {
    // Changing the confidentiality or calling CheckRunningScreenShares() posts
    // a delayed task to resume the screen share to the default task runner. By
    // posting another task to the same runner with the same delay, we guarantee
    // that the expectations are checked at the right time, as the tasks are
    // executed in a sequenced manner.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kScreenShareResumeDelayForTesting);
    run_loop.Run();
  }

  testing::StrictMock<
      base::MockCallback<content::MediaStreamUI::StateChangeCallback>>
      state_change_cb_;
  testing::StrictMock<base::MockCallback<base::RepeatingClosure>> stop_cb_;
  testing::StrictMock<
      base::MockCallback<content::MediaStreamUI::SourceCallback>>
      source_cb_;

 private:
  void MaybeStartScreenShare(std::unique_ptr<MediaAccessHandler> handler,
                             content::WebContents* web_contents,
                             content::MediaStreamRequest request,
                             const content::DesktopMediaID& media_id,
                             bool expect_allowed,
                             bool expect_warning) {
    // First check for the permission to start screen sharing.
    // It should call DlpContentManager::CheckScreenShareRestriction().
    blink::mojom::MediaStreamRequestResult received_result =
        blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS;
    base::RunLoop run_loop;
    handler->HandleRequest(
        web_contents, request,
        base::BindLambdaForTesting(
            [&received_result, &run_loop](
                const blink::mojom::StreamDevicesSet&,
                blink::mojom::MediaStreamRequestResult result,
                std::unique_ptr<content::MediaStreamUI>) {
              received_result = result;
              run_loop.Quit();
            }),
        /*extension=*/nullptr);

    if (expect_warning)
      DismissDialog(expect_allowed);

    run_loop.Run();
    EXPECT_EQ(
        received_result,
        (expect_allowed
             ? blink::mojom::MediaStreamRequestResult::OK
             : blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED));

    // Simulate starting screen sharing.
    // Calls DlpContentManager::OnScreenShareStarted().
    if (expect_allowed) {
      DlpContentManagerAsh* manager =
          static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());

      manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                    stop_cb_.Get(), state_change_cb_.Get(),
                                    source_cb_.Get());
    }
  }
};

// Tests that screenshare is correctly paused for visibility changes of
// Lacros-like windows (Exo surfaces).
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareExoSurface) {
  SetupReporting();

  // Create a Lacros-like Exo surface.
  exo::WMHelper wm_helper;
  std::unique_ptr<exo::ShellSurface> shell_surface =
      exo::test::ShellSurfaceBuilder({640, 480}).BuildShellSurface();
  shell_surface->root_surface()->window()->TrackOcclusionState();

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  exo::SetShellApplicationId(shell_surface->GetWidget()->GetNativeWindow(),
                             kWindowId);
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenShareRestricted);
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;

  // Run for fullscreen and window share.
  const auto root_media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN,
      browser()->window()->GetNativeWindow()->GetRootWindow());
  const auto window_media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW,
      shell_surface->GetWidget()->GetNativeWindow());
  for (const auto media_id : {root_media_id, window_media_id}) {
    // Hide the confidential data.
    shell_surface->GetWidget()->Hide();

    // Setup callbacks to expect a single PAUSE call.
    EXPECT_CALL(stop_cb, Run()).Times(0);
    EXPECT_CALL(state_change_cb,
                Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
        .Times(1);
    manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                  stop_cb.Get(), state_change_cb.Get(),
                                  base::DoNothing());
    // Show the confidential data.
    shell_surface->GetWidget()->Show();
    manager->OnScreenShareStopped(kLabel, media_id);
  }
}

// Tests if screenshare restriction on a Lacros-like windows (Exo surfaces) is
// working if the restriction is sent to ash before the window gets initialized
// there.
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareExoSurfaceCachedRestrictions) {
  SetupReporting();

  // Create a Lacros-like Exo surface.
  exo::WMHelper wm_helper;
  std::unique_ptr<exo::ShellSurface> shell_surface =
      exo::test::ShellSurfaceBuilder({640, 480})
          .SetNoCommit()
          .BuildShellSurface();
  shell_surface->root_surface()->Commit();
  shell_surface->root_surface()->window()->TrackOcclusionState();

  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenshotRestricted);
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kEmptyRestrictionSet);
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenShareRestricted);
  exo::SetShellApplicationId(shell_surface->GetWidget()->GetNativeWindow(),
                             kWindowId);
  shell_surface->root_surface()->Commit();
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;

  // Run for fullscreen and window share.
  const auto root_media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN,
      browser()->window()->GetNativeWindow()->GetRootWindow());
  const auto window_media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW,
      shell_surface->GetWidget()->GetNativeWindow());
  for (const auto media_id : {root_media_id, window_media_id}) {
    // Hide the confidential data.
    shell_surface->GetWidget()->Hide();

    // Setup callbacks to expect a single PAUSE call.
    EXPECT_CALL(stop_cb, Run()).Times(0);
    EXPECT_CALL(state_change_cb,
                Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
        .Times(1);
    manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                  stop_cb.Get(), state_change_cb.Get(),
                                  base::DoNothing());
    // Show the confidential data.
    shell_surface->GetWidget()->Show();
    manager->OnScreenShareStopped(kLabel, media_id);
  }
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareNotification) {
  helper_->SetScreenShareResumeDelay(kScreenShareResumeDelayForTesting);
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  testing::InSequence s;
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);

  auto media_id = MaybeStartFullScreenShare(web_contents);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 0);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 0);

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              web_contents->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 0);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
  WaitForScreenShareResume();

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 1);

  StopScreenShare(media_id);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              web_contents->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareStoppedForSourceChange) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));
  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb_.Get(), state_change_cb_.Get(),
                                base::DoNothing());

  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);

  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);

  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              web_contents->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kBlock, 1u);
  EXPECT_TRUE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 0);

  // Open new tab and navigate to a url.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGoogleUrl)));
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(new_web_contents->GetLastCommittedURL(), GURL(kGoogleUrl));
  const content::DesktopMediaID new_media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          new_web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          new_web_contents->GetPrimaryMainFrame()->GetRoutingID()));
  // Simulate changing the source to another tab.
  manager->OnScreenShareSourceChanging(
      kLabel, media_id, new_media_id,
      /*captured_surface_control_active=*/false);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  manager->OnScreenShareStopped(kLabel, media_id);
  manager->OnScreenShareStarted(kLabel, {new_media_id}, kApplicationTitle,
                                base::DoNothing(), base::DoNothing(),
                                base::DoNothing());

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      false, 1);
  ASSERT_EQ(events_.size(), 1u);
}

struct ScreenShareTestParams {
  ScreenShareTestParams(std::string test_name,
                        DlpRulesManager::Level level,
                        int blocked_count,
                        int warned_count,
                        int total_count,
                        size_t report_event_count,
                        bool expect_allowed,
                        bool expect_warning_proceeded,
                        int paused_count = 0,
                        int resumed_count = 0,
                        int stopped_count = 0)
      : test_name(std::move(test_name)),
        level(level),
        restriction_set(DlpContentRestriction::kScreenShare, level),
        blocked_count(blocked_count),
        warned_count(warned_count),
        total_count(total_count),
        report_event_count(report_event_count),
        expect_allowed(expect_allowed),
        expect_warning_proceeded(expect_warning_proceeded),
        paused_count(paused_count),
        resumed_count(resumed_count),
        stopped_count(stopped_count) {}

  ~ScreenShareTestParams() = default;

  std::string test_name;
  DlpRulesManager::Level level;
  DlpContentRestrictionSet restriction_set;
  // Numbers of expected block, warn, and total UMA histogram points.
  int blocked_count;
  int warned_count;
  int total_count;
  // Number of expected report events. Note that this can differ from
  // total_counts (UMA) as report events are deduplicated.
  size_t report_event_count;
  bool expect_allowed;
  bool expect_warning_proceeded;
  // Numbers of expected callback invocations.
  int paused_count;
  int resumed_count;
  int stopped_count;
};

class CheckAndStartScreenShareTest
    : public DlpContentManagerAshScreenShareBrowserTest,
      public testing::WithParamInterface<ScreenShareTestParams> {};

using CheckRunningScreenShareTest = CheckAndStartScreenShareTest;

IN_PROC_BROWSER_TEST_P(CheckAndStartScreenShareTest, FullScreenShare) {
  const ScreenShareTestParams& param = GetParam();
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, param.restriction_set);

  MaybeStartFullScreenShare(web_contents, param.expect_allowed,
                            /*expect_warning=*/param.warned_count > 0);

  // Notification is only shown in block mode.
  EXPECT_EQ(
      display_service_tester.GetNotification(kScreenShareBlockedNotificationId)
          .has_value(),
      param.blocked_count > 0);

  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  ASSERT_EQ(events_.size(), param.report_event_count);
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                              data_controls::CreateDlpPolicyEvent(
                                  web_contents->GetLastCommittedURL().spec(),
                                  DlpRulesManager::Restriction::kScreenShare,
                                  kRuleName, kRuleId, param.level)));

  if (param.expect_warning_proceeded) {
    EXPECT_THAT(
        events_[1],
        data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
            web_contents->GetLastCommittedURL().spec(),
            DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId)));
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        true, 1);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        false, 0);
  }

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_P(CheckAndStartScreenShareTest, TabShare) {
  const ScreenShareTestParams& param = GetParam();
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, param.restriction_set);

  MaybeStartTabShare(web_contents, param.expect_allowed,
                     /*expect_warning=*/param.warned_count > 0);

  // Notification is only shown in block mode.
  EXPECT_EQ(
      display_service_tester.GetNotification(kScreenShareBlockedNotificationId)
          .has_value(),
      param.blocked_count > 0);

  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  ASSERT_EQ(events_.size(), param.report_event_count);
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                              data_controls::CreateDlpPolicyEvent(
                                  web_contents->GetLastCommittedURL().spec(),
                                  DlpRulesManager::Restriction::kScreenShare,
                                  kRuleName, kRuleId, param.level)));

  if (param.expect_warning_proceeded) {
    EXPECT_THAT(
        events_[1],
        data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
            web_contents->GetLastCommittedURL().spec(),
            DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId)));
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        true, 1);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        false, 0);
  }

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_P(CheckRunningScreenShareTest, FullScreenShare) {
  const ScreenShareTestParams& param = GetParam();
  helper_->SetScreenShareResumeDelay(kScreenShareResumeDelayForTesting);
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MaybeStartFullScreenShare(web_contents, /*expect_allowed=*/true,
                            /*expect_warning=*/false);
  // Nothing is emitted yet since there's no restrictions on web_contents.
  ASSERT_EQ(events_.size(), 0u);
  VerifyHistogramCounts(
      /*blocked_count=*/0,
      /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  testing::InSequence s;
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(param.paused_count);
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(param.resumed_count);
  EXPECT_CALL(stop_cb_, Run).Times(param.stopped_count);

  helper_->ChangeConfidentiality(web_contents, param.restriction_set);
  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  if (param.warned_count > 0)
    DismissDialog(param.expect_allowed);

  // Paused notification is only shown in block mode.
  EXPECT_EQ(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId)
          .has_value(),
      param.blocked_count > 0);

  if (param.expect_warning_proceeded) {
    EXPECT_THAT(
        events_[1],
        data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
            web_contents->GetLastCommittedURL().spec(),
            DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId)));
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        true, 1);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        false, 0);
  }

  // Confirm calls to CheckRunningScreenShares() is ignored if there are no
  // changes in confidentiality: there shouldn't be any new UMA or report
  // events.
  helper_->CheckRunningScreenShares();
  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
  WaitForScreenShareResume();
}

IN_PROC_BROWSER_TEST_P(CheckRunningScreenShareTest, TabShare) {
  const ScreenShareTestParams& param = GetParam();
  helper_->SetScreenShareResumeDelay(kScreenShareResumeDelayForTesting);
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MaybeStartTabShare(web_contents, /*expect_allowed=*/true,
                     /*expect_warning=*/false);
  // Nothing is emitted yet since there's no restrictions on web_contents.
  ASSERT_EQ(events_.size(), 0u);
  VerifyHistogramCounts(
      /*blocked_count=*/0,
      /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  testing::InSequence s;
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(param.paused_count);
  EXPECT_CALL(stop_cb_, Run).Times(param.stopped_count);
  // For resuming tab shares we do not use the state_change_cb_ but the
  // source_cb_ instead:
  EXPECT_CALL(source_cb_, Run).Times(param.resumed_count);

  helper_->ChangeConfidentiality(web_contents, param.restriction_set);
  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  if (param.warned_count > 0)
    DismissDialog(param.expect_allowed);

  // Paused notification is only shown in block mode.
  EXPECT_EQ(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId)
          .has_value(),
      param.blocked_count > 0);

  if (param.expect_warning_proceeded) {
    EXPECT_THAT(
        events_[1],
        data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
            web_contents->GetLastCommittedURL().spec(),
            DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId)));
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        true, 1);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kScreenShareWarnProceededUMA,
        false, 0);
  }

  // Confirm calls to CheckRunningScreenShares() is ignored if there are no
  // changes in confidentiality: there shouldn't be any new UMA or report
  // events.
  helper_->CheckRunningScreenShares();
  VerifyHistogramCounts(
      param.blocked_count, param.warned_count, param.total_count,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
  WaitForScreenShareResume();
}

// Tests that a paused screen share is resumed when the user navigates to
// content that's under warn restriction, but has already allowed sharing it.
IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareResumedWhenNavigatingToBypassedContent) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MaybeStartFullScreenShare(web_contents, /*expect_allowed=*/true,
                            /*expect_warning=*/false);
  // Nothing is emitted yet since there's no restrictions on web_contents.
  ASSERT_EQ(events_.size(), 0u);
  VerifyHistogramCounts(
      /*blocked_count=*/0,
      /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  testing::InSequence s;
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);
  EXPECT_CALL(stop_cb_, Run).Times(0);

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  VerifyHistogramCounts(
      /*blocked_count=*/0,
      /*warned_count=*/1,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  DismissDialog(/*allow=*/true);

  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);
  helper_->ChangeConfidentiality(web_contents, kScreenShareRestricted);
  EXPECT_TRUE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId)
          .has_value());
  VerifyHistogramCounts(
      /*blocked_count=*/1,
      /*warned_count=*/1,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  VerifyHistogramCounts(
      /*blocked_count=*/1,
      /*warned_count=*/2,
      /*total_count=*/4,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      false, 0);

  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ContentsUpdatedOnWebContentsTitleChanged) {
  SetupReporting();
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetMockDlpWarnNotifier();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_CALL(state_change_cb_, Run).Times(1);
  MaybeStartFullScreenShare(web_contents);

  DlpConfidentialContents expected_contents;
  expected_contents.Add(web_contents);
  testing::InSequence s;
  EXPECT_CALL(*mock_dlp_warn_notifier,
              ShowDlpWarningDialog(testing::_,
                                   DlpWarnDialog::DlpWarnDialogOptions(
                                       DlpWarnDialog::Restriction::kScreenShare,
                                       expected_contents, kApplicationTitle)))
      .Times(1);
  expected_contents.GetContents().begin()->title = u"New Title";
  EXPECT_CALL(*mock_dlp_warn_notifier,
              ShowDlpWarningDialog(testing::_,
                                   DlpWarnDialog::DlpWarnDialogOptions(
                                       DlpWarnDialog::Restriction::kScreenShare,
                                       expected_contents, kApplicationTitle)))
      .Times(1);

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  ASSERT_FALSE(helper_->GetRunningScreenShares().empty());
  auto actual_contents = helper_->GetRunningScreenShares()
                             .begin()
                             ->get()
                             ->GetConfidentialContents()
                             .GetContents();
  EXPECT_EQ(actual_contents.size(), 1u);
  EXPECT_EQ(actual_contents.begin()->title, u"example.com");

  // Another check should be ignored if contents don't change.
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  // Change the title.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
            document.title = 'New Title';
  )"));

  ASSERT_FALSE(helper_->GetRunningScreenShares().empty());
  actual_contents = helper_->GetRunningScreenShares()
                        .begin()
                        ->get()
                        ->GetConfidentialContents()
                        .GetContents();
  EXPECT_EQ(actual_contents.size(), 1u);
  EXPECT_EQ(actual_contents.begin()->title, u"New Title");
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
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

  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);

  // Start screen share of the first window.
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW, browser1_window);
  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb_.Get(), state_change_cb_.Get(),
                                /*source_callback=*/base::DoNothing());

  // Move restricted tab from second window to shared first window.
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser2->tab_strip_model()->DetachTabAtForInsertion(0);
  browser1->tab_strip_model()->InsertDetachedTabAt(0, std::move(detached_tab),
                                                   AddTabTypes::ADD_NONE);
  browser1->tab_strip_model()->ActivateTabAt(0);

  // Cleanup and check reporting.
  browser2->window()->Close();
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenSharePausedOrResumedUMA,
      true, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              web_contents2->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kBlock, 1u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       WarningIsShownOnlyOnce) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);

  auto media_id = MaybeStartFullScreenShare(
      web_contents, /*expect_allowed=*/true, /*expect_warning=*/true);
  ASSERT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      true, 1);

  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnSilentProceededUMA,
      true, 1);

  // Since contents allowed by the user are cached, further checks do not
  // trigger a new warning. We have to switch the level as calls to
  // CheckRunningScreenShares() are ignored if there are no changes.
  helper_->ChangeConfidentiality(web_contents, kEmptyRestrictionSet);
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/3,
      /*total_count=*/4,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  StopScreenShare(media_id);
  // Caching should persist over multiple screen shares.
  MaybeStartFullScreenShare(web_contents, /*expect_allowed=*/true,
                            /*expect_warning=*/false);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/5,
      /*total_count=*/6,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
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
  testing::InSequence s;
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb_.Get(), state_change_cb_.Get(),
                                /*source_callback=*/base::DoNothing());
  exo::SetShellApplicationId(browser()->window()->GetNativeWindow(), kWindowId);
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  DismissDialog(/*allow=*/true);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);

  // The window contents should already be cached as allowed by the user, so
  // this should not trigger a new warning. // TODO: this is ignored due to no
  // change
  manager->OnWindowRestrictionChanged(kReceiverId, kWindowId,
                                      kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
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

  MaybeStartTabShare(web_contents);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              web_contents->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kReport, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

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
              web_contents->GetLastCommittedURL().spec(),
              DlpRulesManager::Level::kReport, 1u);
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenShareBlockedNotificationId));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerAshScreenShareBrowserTest,
                       ScreenShareWithoutLabelNotReported) {
  SetupReporting();
  const GURL origin(kExampleUrl);
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId);
  DlpContentManagerAsh* manager =
      static_cast<DlpContentManagerAsh*>(helper_->GetContentManager());
  manager->OnScreenShareStarted("", {media_id}, kApplicationTitle,
                                stop_cb_.Get(), state_change_cb_.Get(),
                                source_cb_.Get());

  helper_->ChangeConfidentiality(web_contents, kScreenShareReported);
  ASSERT_TRUE(events_.empty());
}

struct ScreenshareNavigateTestParams {
  ScreenshareNavigateTestParams(std::string test_name,
                                DlpRulesManager::Level level,
                                std::string histogram_suffix)
      : test_name(std::move(test_name)),
        level(level),
        restriction_set(DlpContentRestriction::kScreenShare, level),
        histogram_suffix(histogram_suffix) {}

  ~ScreenshareNavigateTestParams() = default;

  std::string test_name;
  DlpRulesManager::Level level;
  DlpContentRestrictionSet restriction_set;
  std::string histogram_suffix;
};

class ScreenShareNavigateWebContentsTest
    : public DlpContentManagerAshScreenShareBrowserTest,
      public testing::WithParamInterface<ScreenshareNavigateTestParams> {
 public:
  ScreenShareNavigateWebContentsTest() = default;
  ~ScreenShareNavigateWebContentsTest() override = default;
};

// Tests that navigating between unrestricted, restricted, and only reported
// content during a tab share emits the correct number of reporting events.
IN_PROC_BROWSER_TEST_P(ScreenShareNavigateWebContentsTest, Reporting) {
  const ScreenshareNavigateTestParams& param = GetParam();

  helper_->SetScreenShareResumeDelay(kScreenShareResumeDelayForTesting);
  SetupReporting();
  const GURL restricted_url(kGoogleUrl);
  const GURL reported_url(kExampleUrl);
  const GURL unrestricted_url(kChromeUrl);

  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start sharing unrestricted content.
  helper_->UpdateConfidentiality(web_contents, kEmptyRestrictionSet);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unrestricted_url));
  MaybeStartTabShare(web_contents);

  // Although the share should be paused and resumed, DLP will only call
  // state_change_cb_ once to pause it. When it's supposed to be resumed, it
  // will call source_cb only first and resume only the new stream once
  // notified.
  EXPECT_CALL(state_change_cb_,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(source_cb_, Run).Times(1);

  //   Navigate to reported content. Should emit a report event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), reported_url));
  helper_->UpdateConfidentiality(web_contents, kScreenShareReported);
  helper_->CheckRunningScreenShares();

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          web_contents->GetLastCommittedURL().spec(),
          DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId,
          DlpRulesManager::Level::kReport)));

  // Navigate to unrestricted content. Should not emit any events.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unrestricted_url));
  helper_->UpdateConfidentiality(web_contents, kEmptyRestrictionSet);
  helper_->CheckRunningScreenShares();
  ASSERT_EQ(events_.size(), 1u);

  // Navigate to the previous reported content. Should not emit any report
  // event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), reported_url));
  helper_->UpdateConfidentiality(web_contents, kScreenShareReported);
  helper_->CheckRunningScreenShares();
  ASSERT_EQ(events_.size(), 1u);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kScreenSharePausedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareBlockedUMA,
      true, 0);
  EXPECT_GT(histogram_tester_.GetBucketCount(
                data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kScreenShareBlockedUMA,
                false),
            0);

  // Navigate to restricted content. Should emit a corresponding event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), restricted_url));
  helper_->UpdateConfidentiality(web_contents, param.restriction_set);
  helper_->CheckRunningScreenShares();
  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1], data_controls::IsDlpPolicyEvent(
                              data_controls::CreateDlpPolicyEvent(
                                  GURL(restricted_url).spec(),
                                  DlpRulesManager::Restriction::kScreenShare,
                                  kRuleName, kRuleId, param.level)));
  if (param.level == DlpRulesManager::Level::kWarn) {
    // Proceed, as otherwise the screen share would be stopped.
    DismissDialog(/*allow=*/true);
  } else {
    // Paused notification is only shown in block mode.
    EXPECT_TRUE(display_service_tester.GetNotification(
        kScreenSharePausedNotificationId));
  }
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() + param.histogram_suffix, true, 1);

  // Remember current number of reporting events: further navigation should not
  // emit any new events.
  auto prev_events_size = events_.size();
  // Navigate to the previous reported content. Should not emit any reporting
  // event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), reported_url));
  helper_->UpdateConfidentiality(web_contents, kScreenShareReported);
  helper_->CheckRunningScreenShares();
  EXPECT_EQ(events_.size(), prev_events_size);
  WaitForScreenShareResume();

  // Expect resume notification.
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenShareResumedNotificationId));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() + param.histogram_suffix, true, 1);
}

INSTANTIATE_TEST_SUITE_P(
    DlpContentManagerAsh,
    ScreenshotTest,
    testing::ValuesIn<ScreenshotTestParams>({
        ScreenshotTestParams(/*test_name=*/"Restricted",
                             /*level=*/DlpRulesManager::Level::kBlock,
                             /*blocked_counts=*/{0, 3, 4, 7},
                             /*warned_counts=*/{0, 0, 0, 0},
                             /*total_counts=*/{0, 3, 4, 7},
                             /*report_event_counts=*/{0u, 3u, 4u, 7u},
                             /*expect_allowed=*/false),
        ScreenshotTestParams(/*test_name=*/"WarnedAllowed",
                             /*level=*/DlpRulesManager::Level::kWarn,
                             /*blocked_counts=*/{0, 0, 0, 0},
                             /*warned_counts=*/{0, 3, 4, 7},
                             /*total_counts=*/{0, 3, 4, 7},
                             /*report_event_counts=*/{0u, 1u, 1u, 1u},
                             /*expect_allowed=*/true,
                             /*warning_dialog_count=*/1),
        ScreenshotTestParams(/*test_name=*/"WarnedCanceled",
                             /*level=*/DlpRulesManager::Level::kWarn,
                             /*blocked_counts=*/{0, 0, 0, 0},
                             /*warned_counts=*/{0, 3, 4, 7},
                             /*total_counts=*/{0, 3, 4, 7},
                             /*report_event_counts=*/{0u, 3u, 4u, 7u},
                             /*expect_allowed=*/false,
                             /*warning_dialog_count=*/7),
        ScreenshotTestParams(
            /*test_name=*/"Reported",
            /*level=*/DlpRulesManager::Level::kReport,
            /*blocked_counts=*/{0, 0, 0, 0},
            /*warned_counts=*/{0, 0, 0, 0},
            /*total_counts=*/{0, 0, 0, 0},
            // Calls toCheckScreenshotRestriction() should not be reported if
            // allowed:
            /*report_event_counts=*/{0u, 0u, 0u, 0u},
            /*expect_allowed=*/true),
    }),
    [](const testing::TestParamInfo<ScreenshotTestParams>& info) {
      return info.param.test_name;
    });

INSTANTIATE_TEST_SUITE_P(
    DlpContentManagerAsh,
    CheckAndStartScreenShareTest,
    testing::ValuesIn<ScreenShareTestParams>({
        ScreenShareTestParams(/*test_name=*/"Restricted",
                              /*level=*/DlpRulesManager::Level::kBlock,
                              /*blocked_count=*/1,
                              /*warned_count=*/0,
                              /*total_count=*/1,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/false,
                              /*expect_warning_proceeded=*/false),
        ScreenShareTestParams(/*test_name=*/"WarnedAllowed",
                              /*level=*/DlpRulesManager::Level::kWarn,
                              /*blocked_count=*/0,
                              /*warned_count=*/2,
                              /*total_count=*/2,
                              /*report_event_count=*/2u,
                              /*expect_allowed=*/true,
                              /*expect_warning_proceeded=*/true),
        ScreenShareTestParams(/*test_name=*/"WarnedCanceled",
                              /*level=*/DlpRulesManager::Level::kWarn,
                              /*blocked_count=*/0,
                              /*warned_count=*/1,
                              /*total_count=*/1,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/false,
                              /*expect_warning_proceeded=*/false),
        ScreenShareTestParams(/*test_name=*/"Reported",
                              /*level=*/DlpRulesManager::Level::kReport,
                              /*blocked_count=*/0,
                              /*warned_count=*/0,
                              /*total_count=*/2,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/true,
                              /*expect_warning_proceeded=*/false),
    }),
    [](const testing::TestParamInfo<ScreenShareTestParams>& info) {
      return info.param.test_name;
    });

INSTANTIATE_TEST_SUITE_P(
    DlpContentManagerAsh,
    CheckRunningScreenShareTest,
    testing::ValuesIn<ScreenShareTestParams>({
        ScreenShareTestParams(/*test_name=*/"Restricted",
                              /*level=*/DlpRulesManager::Level::kBlock,
                              /*blocked_count=*/1,
                              /*warned_count=*/0,
                              /*total_count=*/2,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/false,
                              /*expect_warning_proceeded=*/false,
                              /*paused_count=*/1,
                              /*resumed_count=*/1,
                              /*stopped_count=*/0),
        ScreenShareTestParams(/*test_name=*/"WarnedAllowed",
                              /*level=*/DlpRulesManager::Level::kWarn,
                              /*blocked_count=*/0,
                              /*warned_count=*/1,
                              /*total_count=*/2,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/true,
                              /*expect_warning_proceeded=*/true,
                              /*paused_count=*/1,
                              /*resumed_count=*/1,
                              /*stopped_count=*/0),
        ScreenShareTestParams(/*test_name=*/"WarnedCanceled",
                              /*level=*/DlpRulesManager::Level::kWarn,
                              /*blocked_count=*/0,
                              /*warned_count=*/1,
                              /*total_count=*/2,
                              /*report_event_count=*/1u,
                              /*expect_allowed=*/false,
                              /*expect_warning_proceeded=*/false,
                              /*paused_count=*/1,
                              /*resumed_count=*/0,
                              /*stopped_count=*/1),
        ScreenShareTestParams(/*test_name=*/"Reported",
                              /*level=*/DlpRulesManager::Level::kReport,
                              /*blocked_count=*/0,
                              /*warned_count=*/0,
                              /*total_count=*/2,
                              /*report_event_count=*/0u,
                              /*expect_allowed=*/true,
                              /*expect_warning_proceeded=*/false,
                              /*paused_count=*/0,
                              /*resumed_count=*/0,
                              /*stopped_count=*/0),
    }),
    [](const testing::TestParamInfo<ScreenShareTestParams>& info) {
      return info.param.test_name;
    });

INSTANTIATE_TEST_SUITE_P(
    DlpContentManagerAsh,
    ScreenShareNavigateWebContentsTest,
    testing::ValuesIn<ScreenshareNavigateTestParams>(
        {ScreenshareNavigateTestParams(
             /*test_name=*/"Restricted",
             /*level=*/DlpRulesManager::Level::kBlock,
             /*histogram_suffix=*/data_controls::dlp::kScreenShareBlockedUMA),
         ScreenshareNavigateTestParams(
             /*test_name=*/"Warned",
             /*level=*/DlpRulesManager::Level::kWarn,
             /*histogram_suffix=*/data_controls::dlp::kScreenShareWarnedUMA)}),
    [](const testing::TestParamInfo<ScreenshareNavigateTestParams>& info) {
      return info.param.test_name;
    });

}  // namespace policy
