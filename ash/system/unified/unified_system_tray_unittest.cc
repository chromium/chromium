// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/model/fake_power_status.h"
#include "ash/system/model/scoped_fake_power_status.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

constexpr int kQsDetailedViewHeight = 464;
constexpr char kQuickSettingsPageCountOnClose[] =
    "Ash.QuickSettings.PageCountOnClose";

}  // namespace

using message_center::MessageCenter;
using message_center::Notification;

class UnifiedSystemTrayTest : public AshTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  UnifiedSystemTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedSystemTrayTest(const UnifiedSystemTrayTest&) = delete;
  UnifiedSystemTrayTest& operator=(const UnifiedSystemTrayTest&) = delete;
  ~UnifiedSystemTrayTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;

    if (IsVcControlsUiEnabled()) {
      fake_video_conference_tray_controller_ =
          std::make_unique<FakeVideoConferenceTrayController>();
      enabled_features.push_back(features::kFeatureManagementVideoConference);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();

    if (IsVcControlsUiEnabled()) {
      fake_video_conference_tray_controller_.reset();
    }
  }

  bool IsVcControlsUiEnabled() { return GetParam(); }

 protected:
  const std::string AddNotification() {
    const std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test title",
            u"test message", ui::ImageModel(),
            std::u16string() /* display_source */, GURL(),
            message_center::NotifierId(),
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
    return id;
  }

  void RemoveNotification(const std::string id) {
    MessageCenter::Get()->RemoveNotification(id, /*by_user=*/false);
  }

  // Show the notification center bubble. This assumes that there is at least
  // one notification in the notification list.
  void ShowNotificationBubble() {
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->shelf()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->ShowBubble();
  }

  // Hide the notification center bubble. This assumes that it is already
  // shown.
  void HideNotificationBubble() {
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->shelf()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->CloseBubble();
  }

  bool IsBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsBubbleShown();
  }

  bool IsSliderBubbleShown() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->bubble_widget_;
  }

  UnifiedSliderBubbleController::SliderType GetSliderBubbleType() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->slider_type_;
  }

  bool IsMicrophoneMuteToastShown() {
    return IsSliderBubbleShown() &&
           GetSliderBubbleType() ==
               UnifiedSliderBubbleController::SLIDER_TYPE_MIC;
  }

  UnifiedSystemTrayBubble* GetUnifiedSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble_.get();
  }

  void UpdateAutoHideStateNow() {
    GetPrimaryShelf()->shelf_layout_manager()->UpdateAutoHideStateNow();
  }

  gfx::Rect GetBubbleViewBounds() {
    auto* bubble = GetPrimaryUnifiedSystemTray()
                       ->slider_bubble_controller_->bubble_view_.get();
    return bubble ? bubble->GetBoundsInScreen() : gfx::Rect();
  }

  void TransferFromCalendarViewToMainViewByFuncKeys(UnifiedSystemTray* tray,
                                                    TrayBubbleView* bubble_view,
                                                    ui::KeyboardCode key) {
    ShellTestApi().PressAccelerator(ui::Accelerator(key, ui::EF_NONE));
    EXPECT_FALSE(tray->IsShowingCalendarView());
    // Tests that `UnifiedSystemTray` is active and has the ink drop, while
    // `DateTray` becomes inactive.
    EXPECT_TRUE(tray->is_active());
    EXPECT_FALSE(date_tray()->is_active());
    // The main bubble is shorter than the detailed view bubble.
    EXPECT_GT(kQsDetailedViewHeight, bubble_view->height());
  }

  void CheckDetailedViewHeight(TrayBubbleView* bubble_view) {
    // The bubble height should be fixed to the detailed view height.
    EXPECT_EQ(kQsDetailedViewHeight, bubble_view->height());
  }

  TimeTrayItemView* time_view() {
    return GetPrimaryUnifiedSystemTray()->time_view_;
  }

  ImeModeView* ime_mode_view() {
    return GetPrimaryUnifiedSystemTray()->ime_mode_view_;
  }

  DateTray* date_tray() {
    return Shell::GetPrimaryRootWindowController()
        ->shelf()
        ->GetStatusAreaWidget()
        ->date_tray();
  }

  FakeVideoConferenceTrayController* fake_video_conference_tray_controller() {
    return fake_video_conference_tray_controller_.get();
  }

 private:
  int id_ = 0;

  std::unique_ptr<FakeVideoConferenceTrayController>
      fake_video_conference_tray_controller_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedSystemTrayTest,
                         testing::Bool() /*IsVcControlsUiEnabled()*/);

// Regression test for crbug/1360579
TEST_P(UnifiedSystemTrayTest, GetAccessibleNameForQuickSettingsBubble) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  EXPECT_EQ(tray->GetAccessibleNameForQuickSettingsBubble(),
            l10n_util::GetStringUTF16(
                IDS_ASH_QUICK_SETTINGS_BUBBLE_ACCESSIBLE_DESCRIPTION));
}

TEST_P(UnifiedSystemTrayTest, ShowVolumeSliderBubble) {
  // The volume popup is not visible initially.
  EXPECT_FALSE(IsSliderBubbleShown());

  // When set to autohide, the shelf shouldn't be shown.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status->ShouldShowShelf());

  // Simulate ARC asking to show the volume view.
  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  // Volume view is now visible.
  EXPECT_TRUE(IsSliderBubbleShown());
  EXPECT_EQ(UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME,
            GetSliderBubbleType());

  // This does not force the shelf to automatically show. Regression tests for
  // crbug.com/729188
  EXPECT_FALSE(status->ShouldShowShelf());
}

TEST_P(UnifiedSystemTrayTest, SliderBubbleMovesOnShelfAutohide) {
  // The slider button should be moved when the autohidden shelf is shown, so
  // as to not overlap. Regression test for crbug.com/1136564
  auto* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a test widget to make auto-hiding work. Auto-hidden shelf will
  // remain visible if no windows are shown, making it impossible to properly
  // test.
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto center = display.bounds().CenterPoint();
  auto bottom_center = display.bounds().bottom_center();
  bottom_center.set_y(bottom_center.y() - 1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();

  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  gfx::Rect before_bounds = GetBubbleViewBounds();

  // Now move the mouse close to the edge, so that the shelf shows, and verify
  // that the volume slider adjusts accordingly.
  generator->MoveMouseTo(bottom_center);
  UpdateAutoHideStateNow();
  gfx::Rect after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);

  // Also verify that the shelf and slider bubble would have overlapped, but do
  // not now that we've moved the slider bubble.
  gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  EXPECT_TRUE(before_bounds.Intersects(shelf_bounds));
  EXPECT_FALSE(after_bounds.Intersects(shelf_bounds));

  // Move the mouse away and verify that it adjusts back to its original
  // position.
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Now fullscreen and restore our window with autohide disabled and verify
  // that the bubble moves down as the shelf disappears and reappears. Disable
  // autohide so that the shelf is initially showing.
  shelf->SetAlignment(ShelfAlignment::kRight);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Adjust the alignment of the shelf, and verify that the bubble moves along
  // with it.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  before_bounds = GetBubbleViewBounds();
  widget->SetFullscreen(true);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  widget->SetFullscreen(false);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);
}

TEST_P(UnifiedSystemTrayTest, ShowBubble_MultipleDisplays_OpenedOnSameDisplay) {
  // Initialize two displays with 800x700 resolution.
  UpdateDisplay("400+400-800x600,1220+400-800x600");
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(2, screen->GetNumDisplays());

  // The tray bubble for each display should be opened on the same display.
  // See crbug.com/937420.
  for (int i = 0; i < screen->GetNumDisplays(); ++i) {
    auto* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    const gfx::Rect primary_display_bounds = GetPrimaryDisplay().bounds();
    const gfx::Rect tray_bubble_bounds =
        GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
    EXPECT_TRUE(primary_display_bounds.Contains(tray_bubble_bounds))
        << "primary display bounds=" << primary_display_bounds.ToString()
        << ", tray bubble bounds=" << tray_bubble_bounds.ToString();

    SwapPrimaryDisplay();
  }
}

TEST_P(UnifiedSystemTrayTest, HorizontalImeAndTimeLabelAlignment) {
  ime_mode_view()->label()->SetText(u"US");
  ime_mode_view()->SetVisible(true);

  gfx::Rect time_bounds = time_view()
                              ->time_view()
                              ->GetHorizontalTimeLabelForTesting()
                              ->GetBoundsInScreen();
  gfx::Rect ime_bounds = ime_mode_view()->label()->GetBoundsInScreen();

  EXPECT_EQ(time_bounds.y(), ime_bounds.y());
  EXPECT_EQ(time_bounds.height(), ime_bounds.height());
}

TEST_P(UnifiedSystemTrayTest, FocusQuickSettings) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  auto* quick_settings_view = tray->bubble()->quick_settings_view();
  auto* focus_manager = quick_settings_view->GetFocusManager();
  EXPECT_FALSE(quick_settings_view->Contains(focus_manager->GetFocusedView()));

  // Press the tab key should focus on the first button in the qs bubble.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(quick_settings_view->Contains(focus_manager->GetFocusedView()));
}

TEST_P(UnifiedSystemTrayTest, TimeInQuickSettingsMetric) {
  base::HistogramTester histogram_tester;
  constexpr base::TimeDelta kTimeInQuickSettings = base::Seconds(3);
  auto* tray = GetPrimaryUnifiedSystemTray();

  // Open the tray.
  tray->ShowBubble();

  // Spend cool-down time with tray open.
  task_environment()->FastForwardBy(kTimeInQuickSettings);

  // Close and record the metric.
  tray->CloseBubble();

  // Ensure metric recorded time passed while Quick Setting was open.
  histogram_tester.ExpectTimeBucketCount("Ash.QuickSettings.UserJourneyTime",
                                         kTimeInQuickSettings,
                                         /*count=*/1);

  // Re-open the tray.
  tray->ShowBubble();

  // Metric isn't recorded when adding and removing a notification.
  std::string id = AddNotification();
  RemoveNotification(id);
  histogram_tester.ExpectTotalCount("Ash.QuickSettings.UserJourneyTime",
                                    /*count=*/1);

  // Metric is recorded after closing bubble.
  tray->CloseBubble();
  histogram_tester.ExpectTotalCount("Ash.QuickSettings.UserJourneyTime",
                                    /*count=*/2);
}

// Tests that the number of quick settings pages is recorded when the QS bubble
// is closed.
TEST_P(UnifiedSystemTrayTest, QuickSettingsPageCountMetric) {
  base::HistogramTester histogram_tester;

  // Show the bubble with one page and verify that nothing is recorded yet.
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  tray->bubble()
      ->unified_system_tray_controller()
      ->model()
      ->pagination_model()
      ->SetTotalPages(1);
  histogram_tester.ExpectTotalCount(kQuickSettingsPageCountOnClose, 0);

  // Close the bubble and verify that the metric is recorded.
  tray->CloseBubble();
  histogram_tester.ExpectTotalCount(kQuickSettingsPageCountOnClose, 1);
  histogram_tester.ExpectBucketCount(kQuickSettingsPageCountOnClose,
                                     /*sample=*/1,
                                     /*expected_count=*/1);

  // Show the bubble with two pages, and verify that the metric is recorded when
  // the bubble is closed.
  tray->ShowBubble();
  tray->bubble()
      ->unified_system_tray_controller()
      ->model()
      ->pagination_model()
      ->SetTotalPages(2);
  tray->CloseBubble();
  histogram_tester.ExpectTotalCount(kQuickSettingsPageCountOnClose, 2);
  histogram_tester.ExpectBucketCount(kQuickSettingsPageCountOnClose,
                                     /*sample=*/2,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kQuickSettingsPageCountOnClose,
                                     /*sample=*/1,
                                     /*expected_count=*/1);
}

// Tests that pressing the TOGGLE_CALENDAR accelerator once results in the
// calendar view showing.
TEST_P(UnifiedSystemTrayTest, PressCalendarAccelerator) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());
}

// Tests that pressing the TOGGLE_CALENDAR accelerator twice results in a hidden
// QuickSettings bubble.
TEST_P(UnifiedSystemTrayTest, ToggleCalendarViewAccelerator) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  EXPECT_FALSE(GetUnifiedSystemTrayBubble());
}

// Tests that showing the calendar view by the TOGGLE_CALENDAR accelerator
// results in the CalendarDateCellView being focused.
TEST_P(UnifiedSystemTrayTest, CalendarAcceleratorFocusesDateCell) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  auto* focus_manager =
      GetUnifiedSystemTrayBubble()->GetBubbleWidget()->GetFocusManager();
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_STREQ(focus_manager->GetFocusedView()->GetClassName(),
               "CalendarDateCellView");
}

// Tests that using functional keys to change brightness/volume when the
// `CalendarView` is open will make ink drop transfer and bubble height change.
TEST_P(UnifiedSystemTrayTest, CalendarGoesToMainViewByFunctionalKeys) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* bubble_view = tray->bubble()->GetBubbleView();

  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(tray->IsShowingCalendarView());
  CheckDetailedViewHeight(bubble_view);

  // Tests the volume up/down/mute functional keys. It should hide the calendar
  // view and open the `unified_system_tray_bubble_`. The ink drop should
  // transfer from `DateTray` to `UnifiedSystemTray` and the `bubble_view`
  // should shrink for the Qs main page.
  TransferFromCalendarViewToMainViewByFuncKeys(tray, bubble_view,
                                               ui::VKEY_VOLUME_UP);
  TransferFromCalendarViewToMainViewByFuncKeys(tray, bubble_view,
                                               ui::VKEY_VOLUME_DOWN);
  TransferFromCalendarViewToMainViewByFuncKeys(tray, bubble_view,
                                               ui::VKEY_VOLUME_MUTE);

  // Tests the brightness up/down functional keys.
  TransferFromCalendarViewToMainViewByFuncKeys(tray, bubble_view,
                                               ui::VKEY_BRIGHTNESS_UP);
  TransferFromCalendarViewToMainViewByFuncKeys(tray, bubble_view,
                                               ui::VKEY_BRIGHTNESS_DOWN);

  tray->CloseBubble();
}

// Tests if the microphone mute toast is displayed when the mute state is
// toggled by the software switches.
TEST_P(UnifiedSystemTrayTest, InputMuteStateToggledBySoftwareSwitch) {
  // The microphone mute toast should not be visible initially.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  CrasAudioHandler* cras_audio_handler = CrasAudioHandler::Get();
  // Toggling the system input mute state using software switches.
  cras_audio_handler->SetInputMute(
      !cras_audio_handler->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kOther);

  // The toast should not be visible as the mute state is toggled using a
  // software switch.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());
}

// Tests if the microphone mute toast is displayed when the mute state is
// toggled by the keyboard switch.
TEST_P(UnifiedSystemTrayTest, InputMuteStateToggledByKeyboardSwitch) {
  // The microphone mute toast should not be visible initially.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  CrasAudioHandler* cras_audio_handler = CrasAudioHandler::Get();
  // Toggling the system input mute state using the dedicated keyboard button.
  cras_audio_handler->SetInputMute(
      !cras_audio_handler->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);

  // The toast should be visible as the mute state is toggled using the keyboard
  // switch.
  EXPECT_TRUE(IsMicrophoneMuteToastShown());
}

// Tests if the microphone mute toast is displayed when the mute state is
// toggled by the hw switch.
TEST_P(UnifiedSystemTrayTest, InputMuteStateToggledByHardwareSwitch) {
  // The microphone mute toast should not be visible initially.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  CrasAudioHandler* cras_audio_handler = CrasAudioHandler::Get();
  // Toggling the input mute state using the hw switch.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      !cras_audio_handler->IsInputMuted());

  // The toast should be visible as the mute state is toggled using the hw
  // switch.
  EXPECT_TRUE(IsMicrophoneMuteToastShown());
}

// Tests if the microphone mute toast is NOT displayed when the mute state is
// toggled by the hw switch and the VC tray is visible.
TEST_P(UnifiedSystemTrayTest,
       InputMuteStateToggledByHardwareSwitchVcTrayVisible) {
  if (!IsVcControlsUiEnabled()) {
    return;
  }

  // The microphone mute toast should not be visible initially.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  // Show the VC tray.
  auto* vc_tray = Shell::Get()
                      ->GetPrimaryRootWindowController()
                      ->shelf()
                      ->GetStatusAreaWidget()
                      ->video_conference_tray();
  DCHECK(vc_tray);

  // Update media state, which will make the `VideoConferenceTray` show.
  VideoConferenceMediaState state;
  state.has_media_app = true;
  fake_video_conference_tray_controller()->UpdateWithMediaState(state);
  ASSERT_TRUE(vc_tray->GetVisible());

  CrasAudioHandler* cras_audio_handler = CrasAudioHandler::Get();
  // Toggling the input mute state using the hw switch.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      !cras_audio_handler->IsInputMuted());

  // The toast should NOT be visible as the mute state is toggled using the hw
  // switch and the VC tray is visible.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  // Make the VC tray not-visible and toggle again, now the toast is visible.
  state.has_media_app = false;
  fake_video_conference_tray_controller()->UpdateWithMediaState(state);
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      !cras_audio_handler->IsInputMuted());
  EXPECT_TRUE(IsMicrophoneMuteToastShown());
}

// Tests microphone mute toast is visible only when the device has an
// internal/external microphone attached.
TEST_P(UnifiedSystemTrayTest, InputMuteStateToggledButNoMicrophoneAvailable) {
  // Creating an input device for simple usage.
  AudioNode internal_mic;
  internal_mic.is_input = true;
  internal_mic.id = 1;
  internal_mic.stable_device_id_v1 = internal_mic.id;
  internal_mic.type = AudioDevice::GetTypeString(AudioDeviceType::kInternalMic);

  // Creating an output device.
  AudioNode internal_speaker;
  internal_speaker.is_input = false;
  internal_speaker.id = 2;
  internal_speaker.stable_device_id_v1 = internal_speaker.id;
  internal_speaker.type =
      AudioDevice::GetTypeString(AudioDeviceType::kInternalSpeaker);

  // The microphone mute toast should not be visible initially.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());

  FakeCrasAudioClient* fake_cras_audio_client = FakeCrasAudioClient::Get();
  CrasAudioHandler* cras_audio_handler = CrasAudioHandler::Get();

  fake_cras_audio_client->SetAudioNodesAndNotifyObserversForTesting(
      {internal_speaker, internal_mic});
  cras_audio_handler->SetInputMute(
      !cras_audio_handler->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  // The toast should be visible as the input mute has changed and there is a
  // microphone for simple usage attached to the device.
  EXPECT_TRUE(IsMicrophoneMuteToastShown());

  fake_cras_audio_client->SetAudioNodesAndNotifyObserversForTesting(
      {internal_speaker});
  cras_audio_handler->SetInputMute(
      !cras_audio_handler->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  // There is no microphone for simple usage attached to the device. The toast
  // should not be displayed even though the input mute has changed in the
  // backend.
  EXPECT_FALSE(IsMicrophoneMuteToastShown());
}

// Tests that the bubble is closed after entering or exiting tablet mode. This
// is required because the `FeatureTile`'s must be recreated to switch between
// primary and compact.
TEST_P(UnifiedSystemTrayTest, BubbleClosedAfterTabletModeChange) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // Show bubble.
  EXPECT_FALSE(IsBubbleShown());
  tray->ShowBubble();
  EXPECT_TRUE(IsBubbleShown());

  // Expect bubble to close after entering tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_FALSE(IsBubbleShown());

  // Show bubble again.
  tray->ShowBubble();
  EXPECT_TRUE(IsBubbleShown());

  // Expect bubble to close after exiting tablet mode.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that the tray background has the correct color when entering tablet
// mode.
TEST_P(UnifiedSystemTrayTest, TrayBackgroundColorAfterSwitchToTabletMode) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  auto* widget = tray->GetWidget();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_EQ(tray->layer()->background_color(),
            ShelfConfig::Get()->GetShelfControlButtonColor(widget));

  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_EQ(tray->layer()->background_color(),
            widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated));

  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_EQ(tray->layer()->background_color(),
            ShelfConfig::Get()->GetShelfControlButtonColor(widget));
}

// Tests that the bubble automatically hides if it is visible when another
// bubble becomes visible, and otherwise does not automatically show or hide.
TEST_P(UnifiedSystemTrayTest, BubbleHideBehavior) {
  // Basic verification test that the unified system tray bubble can show/hide
  // itself when no other bubbles are visible.
  auto* tray = GetPrimaryUnifiedSystemTray();
  EXPECT_FALSE(IsBubbleShown());
  tray->ShowBubble();
  EXPECT_TRUE(IsBubbleShown());
  tray->CloseBubble();
  EXPECT_FALSE(IsBubbleShown());

  // Test that the unified system tray bubble automatically hides when it is
  // currently visible while another bubble becomes visible.
  AddNotification();
  tray->ShowBubble();
  EXPECT_TRUE(IsBubbleShown());
  ShowNotificationBubble();
  EXPECT_FALSE(IsBubbleShown());

  // Hide all currently visible bubbles.
  HideNotificationBubble();
  EXPECT_FALSE(IsBubbleShown());

  // Test that the unified system tray bubble stays hidden when showing another
  // bubble.
  ShowNotificationBubble();
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(UnifiedSystemTrayTest, BubbleViewSizeChangeWithEnoughSpace) {
  // Set a large enough screen size.
  UpdateDisplay("1600x900");

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* bubble_view = tray->bubble()->GetBubbleView();

  // The main page height should be smaller than the detailed view height.
  EXPECT_GT(kQsDetailedViewHeight, bubble_view->height());

  // Goes to a detailed view (here using calendar view).
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Asserts that calendar is actually shown.
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());

  CheckDetailedViewHeight(bubble_view);
  tray->CloseBubble();
}

TEST_P(UnifiedSystemTrayTest, BubbleViewSizeChangeNoEnoughSpace) {
  // Set a small screen size.
  UpdateDisplay("300x200");

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* bubble_view = tray->bubble()->GetBubbleView();

  // The main page height should be smaller than the detailed view height.
  EXPECT_GT(kQsDetailedViewHeight, bubble_view->height());

  // Goes to a detailed view (here using calendar view).
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  // Asserts that calendar is actually shown.
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());

  // No enough space for the fixed detailed view height.
  EXPECT_GT(kQsDetailedViewHeight, bubble_view->height());
  tray->CloseBubble();
}

TEST_P(UnifiedSystemTrayTest, BubbleViewSizeChangeWithBigMainPage) {
  // Set a large enough screen size.
  UpdateDisplay("1600x900");

  // The following code adds 2 more row in the tile section and 1 media view to
  // the qs bubble. In this case the main page should be larger than the default
  // detailed page height.

  // Enables nearby sharing to show the tile.
  auto* test_delegate = static_cast<TestNearbyShareDelegate*>(
      Shell::Get()->nearby_share_delegate());
  test_delegate->set_is_pod_button_visible(true);

  // Constructs the test cast config to add the cast tile.
  TestCastConfigController cast_config;

  // Adds locales to show the locale tile.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back("en-US", u"English (United States)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   "en-US");
  // Adds the media view.
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* qs_view = tray->bubble()->quick_settings_view();
  auto* tray_controller = tray->bubble()->unified_system_tray_controller();
  auto media_controller =
      std::make_unique<QuickSettingsMediaViewController>(tray_controller);

  // Outside tests the QuickSettingsMediaViewController is set in
  // UnifiedSystemTrayController::CreateQuickSettingsView() which is not
  // called here, but we need to reference QuickSettingsMediaViewController in
  // QuickSettingsMediaViewContainer::MaybeShowMediaView() when calling
  // QuickSettingsView::SetShowMediaView(), so we need to manually set the
  // controller for testing.
  tray_controller->SetMediaViewControllerForTesting(
      std::move(media_controller));

  qs_view->AddMediaView(tray_controller->media_view_controller()->CreateView());
  qs_view->SetShowMediaView(true);

  auto* bubble_view = tray->bubble()->GetBubbleView();

  // The main page height should be larger than the detailed view height.
  EXPECT_LT(kQsDetailedViewHeight, bubble_view->height());

  const int main_page_height = bubble_view->height();

  // Goes to a detailed view (here using calendar view).
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Asserts that calendar is actually shown.
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());

  EXPECT_LT(kQsDetailedViewHeight, bubble_view->height());
  EXPECT_EQ(main_page_height, bubble_view->height());

  tray->CloseBubble();
}

// Tests that there's no bubble in the kiosk mode.
TEST_P(UnifiedSystemTrayTest, NoBubbleAndNoDetailedViewInKioskMode) {
  SimulateKioskMode(user_manager::UserType::kKioskApp);

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  // In the kiosk mode, the bubble doesn't exist.
  EXPECT_FALSE(IsBubbleShown());

  // Trying to show any of the detailed view will not show the bubble.
  tray->ShowAudioDetailedViewBubble();
  EXPECT_FALSE(IsBubbleShown());

  tray->ShowNetworkDetailedViewBubble();
  EXPECT_FALSE(IsBubbleShown());

  tray->ShowDisplayDetailedViewBubble();
  EXPECT_FALSE(IsBubbleShown());
}

class PowerTrayViewTest : public UnifiedSystemTrayTest {
 public:
  FakePowerStatus* GetFakePowerStatus() {
    return scoped_fake_power_status_.get()->fake_power_status();
  }

  PowerTrayView* power_tray_view() {
    return GetPrimaryUnifiedSystemTray()->power_tray_view_;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_fake_power_status_ = std::make_unique<ScopedFakePowerStatus>();
  }

  // AshTestBase:
  void TearDown() override {
    scoped_fake_power_status_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<ScopedFakePowerStatus> scoped_fake_power_status_;
};

TEST_F(PowerTrayViewTest, BatteryVisibility) {
  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetIsBatteryPresent(false);

  // OnPowerStatusChanged() is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly to ensure that the visibility
  // is set before the check.
  power_tray_view()->OnPowerStatusChanged();

  EXPECT_FALSE(power_tray_view()->GetVisible());
}

TEST_F(PowerTrayViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  power_tray_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kImage);
}

TEST_F(PowerTrayViewTest, AccessibleName) {
  ui::AXNodeData data;

  power_tray_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      PowerStatus::Get()->GetAccessibleNameString(/* full_description*/ true));

  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetIsBatteryPresent(false);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly to ensure that the visibility
  // is set before the check.
  power_tray_view()->OnPowerStatusChanged();
  data = ui::AXNodeData();

  power_tray_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      PowerStatus::Get()->GetAccessibleNameString(/* full_description*/ true));
}

}  // namespace ash
