// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/channel_indicator/channel_indicator.h"
#include "ash/system/hotspot/hotspot_tray_view.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/fake_power_status.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/scoped_fake_power_status.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/current_locale_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
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

// These values represent a portion of the status string that is used in the
// UnifiedSystemTray's accessible name, and the numerical value represents the
// index of the status string vector where the value is stored. The presence of
// this enum makes testing changes to the string simpler.
enum class StatusType {
  kTime = 0,
  kBattery = 1,
  kChannelIndicator = 2,
  kNetworkHotspot = 3,
  kManagedDevice = 4,
  kImeMode = 5,
  kCurrentLocale = 6,
  kMaxValue = kCurrentLocale
};

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
    return GetPrimaryUnifiedSystemTray()->time_tray_item_view_;
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
  display::Display display = display::Screen::Get()->GetPrimaryDisplay();
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
  auto* screen = display::Screen::Get();
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
  EXPECT_EQ(focus_manager->GetFocusedView()->GetClassName(),
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

TEST_P(UnifiedSystemTrayTest, BrightnessSliderDisabledInDockedMode) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const auto internal_info =
      display_manager()->GetDisplayInfo(internal_display_id);
  constexpr int64_t external_id = 210000010;

  const auto external_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID("400x300", external_id);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(internal_info);
  display_info_list.push_back(external_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  EXPECT_TRUE(tray->bubble()
                  ->unified_system_tray_controller()
                  ->GetBrightnessSliderEnabledForTesting());

  display_info_list.clear();
  display_info_list.push_back(external_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  EXPECT_FALSE(tray->bubble()
                   ->unified_system_tray_controller()
                   ->GetBrightnessSliderEnabledForTesting());
}

// Tests that there's no bubble in the kiosk mode.
TEST_P(UnifiedSystemTrayTest, NoBubbleAndNoDetailedViewInKioskMode) {
  SimulateKioskMode(user_manager::UserType::kKioskChromeApp);

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

TEST_P(UnifiedSystemTrayTest, BubbleViewAccessibleName) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  EXPECT_TRUE(IsBubbleShown());
  auto* bubble_view = tray->GetBubbleView();

  ui::AXNodeData node_data;
  bubble_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            tray->GetAccessibleNameForBubble());
}

// Tests that the bubble bounds are set correctly when the virtual keyboard is
// shown/hidden.
TEST_P(UnifiedSystemTrayTest, VirtualKeyboardBubbleLayout) {
  // Set a large enough screen size.
  UpdateDisplay("1600x900");

  // Start tablet mode and wait until display mode is updated.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();

  KeyboardController* keyboard_controller = KeyboardController::Get();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);
  // The keyboard needs to be in a loaded state before being shown.
  ASSERT_TRUE(keyboard::test::WaitUntilLoaded());

  // Open the QS bubble.
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* bubble_view = tray->bubble()->GetBubbleView();
  tray->bubble()->UpdateBubble();

  // Verify that the keyboard isn't visible.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  gfx::Rect initial_bounds = bubble_view->GetBoundsInScreen();

  // Show the virtual keyboard and verify the bounds of the bubble have changed.
  keyboard_controller->ShowKeyboard();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(keyboard_controller->IsKeyboardVisible());
  EXPECT_NE(bubble_view->GetBoundsInScreen(), initial_bounds);

  // Hide the virtual keyboard and verify that the bubble bounds have been
  // restored.
  keyboard_controller->HideKeyboard(HideReason::kSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  EXPECT_EQ(bubble_view->GetBoundsInScreen(), initial_bounds);

  tray->CloseBubble();
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

class UnifiedSystemTrayAccessibilityTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    // Force the channel to return version_info::Channel::BETA so that the
    // ChannelIndicatorView gets created.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    shell_delegate->set_channel(version_info::Channel::BETA);
    set_shell_delegate(std::move(shell_delegate));
    AshTestBase::SetUp();

    scoped_fake_power_status_ = std::make_unique<ScopedFakePowerStatus>();

    test_clock_.SetNow(TimeFromString("13 Dec 2024 4:00 UTC"));
    GetPrimaryUnifiedSystemTray()->OverrideClockForTesting(&test_clock_);
    GetPrimaryUnifiedSystemTray()->UpdateAccessibleName();
  }

  // AshTestBase:
  void TearDown() override {
    scoped_fake_power_status_.reset();
    AshTestBase::TearDown();
  }

  ChannelIndicatorView* channel_indicator_view() {
    return GetPrimaryUnifiedSystemTray()->channel_indicator_view_;
  }

  FakePowerStatus* GetFakePowerStatus() {
    return scoped_fake_power_status_.get()->fake_power_status();
  }

  PowerTrayView* power_tray_view() {
    return GetPrimaryUnifiedSystemTray()->power_tray_view_;
  }

  NetworkTrayView* network_tray_view() {
    return GetPrimaryUnifiedSystemTray()->network_tray_view_;
  }

  HotspotTrayView* hotspot_tray_view() {
    return GetPrimaryUnifiedSystemTray()->hotspot_tray_view_;
  }

  ImeModeView* ime_mode_view() {
    return GetPrimaryUnifiedSystemTray()->ime_mode_view_;
  }

  CurrentLocaleView* current_locale_view() {
    return GetPrimaryUnifiedSystemTray()->current_locale_view_;
  }

  void CreateDefaultStatusForTesting(std::vector<std::u16string>* status) {
    CreateStatusWithPlaceholders(status);

    // Set up the time component of the "status."
    std::u16string test_time = base::TimeFormatTimeOfDayWithHourClockType(
        test_clock_.Now(),
        Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
        base::kKeepAmPm);
    UpdatePartOfStatus(status, test_time, StatusType::kTime);

    // Set up the battery component of the "status."
    FakePowerStatus* fake_power_status = GetFakePowerStatus();
    EXPECT_FALSE(fake_power_status->proto_initialized());
    UpdatePartOfStatus(
        status,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_CHARGE_LEVEL_ACCESSIBLE),
        StatusType::kBattery);

    // Because we are setting the channel to Beta in `Setup()`, the
    // `channel_indicator_view()` is visible at setup and has a non-empty part
    // of the tray's status. Because we want these tests to test each part of
    // the status in isolation, set the `channel_indicator_view()` to invisible
    // for now so it doesn't impact the other tests. We will set it to visible
    // in the relevant tests.
    channel_indicator_view()->SetVisible(false);
    UpdatePartOfStatus(status, std::u16string(), StatusType::kChannelIndicator);

    // Start with the NetworkTrayView, HotspotTrayView, ManagedDeviceView,
    // ImeModeView, and CurrentLocaleView as not visible for the default
    // string, and update them to visible in specific tests.
    network_tray_view()->SetVisible(false);
    UpdatePartOfStatus(status, std::u16string(), StatusType::kNetworkHotspot);
    UpdatePartOfStatus(status, std::u16string(), StatusType::kManagedDevice);
    UpdatePartOfStatus(status, std::u16string(), StatusType::kImeMode);
    UpdatePartOfStatus(status, std::u16string(), StatusType::kCurrentLocale);
  }

  std::u16string FormatPowerPercentageString(
      int percentage_accessibility_token,
      FakePowerStatus* fake_power_status) {
    return l10n_util::GetStringFUTF16(
        percentage_accessibility_token,
        base::NumberToString16(fake_power_status->GetRoundedBatteryPercent()));
  }

  void UpdatePartOfStatus(std::vector<std::u16string>* status,
                          std::u16string new_string,
                          StatusType status_part) {
    int index = static_cast<int>(status_part);
    status->at(index) = new_string;
  }

  base::Time TimeFromString(const char* time_string) {
    base::Time time;
    CHECK(base::Time::FromString(time_string, &time));
    return time;
  }

  // Manually trigger an accessible name change in the TimeView to trigger the
  // callback to UnifiedSystemTray::UpdateAccessibleName.
  void UpdateTimeViewName() {
    GetPrimaryUnifiedSystemTray()
        ->time_tray_item_view_->time_view()
        ->GetViewAccessibility()
        .SetName(u"Test");
  }

  void RegisterUserWithUserPrefs(const AccountId& account_id,
                                 user_manager::UserType user_type) {
    // Create a fake user prefs map.
    ClearLogin();
    SimulateUserLogin({user_email, user_type}, account_id);
  }

 protected:
  base::SimpleTestClock test_clock_;
  const std::string user_email = "user@mail.com";

 private:
  void CreateStatusWithPlaceholders(std::vector<std::u16string>* status) {
    status->push_back(u"Test time");
    status->push_back(u"Test battery");
    status->push_back(u"Test channel indicator");
    status->push_back(u"Test network and hotspot");
    status->push_back(u"Test managed device");
    status->push_back(u"Test ime mode");
    status->push_back(u"Test current locale");
  }

  std::unique_ptr<ScopedFakePowerStatus> scoped_fake_power_status_;
};

TEST_F(UnifiedSystemTrayAccessibilityTest, BaseName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

TEST_F(UnifiedSystemTrayAccessibilityTest, TimeChangeUpdatesName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  // Mock the clock changing, which would trigger the TimeView's accessible name
  // to change.
  test_clock_.Advance(base::Minutes(1));
  UpdateTimeViewName();

  UpdatePartOfStatus(
      &status,
      base::TimeFormatTimeOfDayWithHourClockType(
          test_clock_.Now(),
          Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
          base::kKeepAmPm),
      StatusType::kTime);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

TEST_F(UnifiedSystemTrayAccessibilityTest, NameWithFullBatteryPower) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  // The default state of the battery is FULL, but the battery percentage needs
  // to be near 100% for the UI to consider the battery actually full.
  power_manager::PowerSupplyProperties prop;
  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetProtoForTesting(prop);
  fake_power_status->SetBatteryPercent(100.0);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly.
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(&status,
                     l10n_util::GetStringUTF16(
                         IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE),
                     StatusType::kBattery);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

// This tests the logic in `PowerStatus::GetAccessibleNameString` where
// `features::IsBatterySaverAvailable()` and `IsBatteryCharging()` are both
// true.
TEST_F(UnifiedSystemTrayAccessibilityTest,
       NameWithBatterySaverEnabledAndCharging) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kBatterySaver);

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  power_manager::PowerSupplyProperties prop;
  prop.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetProtoForTesting(prop);

  // First, test the name is set correctly when `IsBatterySaveryActive()`
  // returns true.
  fake_power_status->SetIsBatterySaverActive(true);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly.
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_BSM_ON_ACCESSIBLE,
          fake_power_status),
      StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }

  // Second, test the name is set correctly when `IsBatterySaveryActive()`
  // returns false.
  fake_power_status->SetIsBatterySaverActive(false);

  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE,
          fake_power_status),
      StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }
}

// This tests the logic in `PowerStatus::GetAccessibleNameString` where
// `features::IsBatterySaverAvailable()` is true and `IsBatteryCharging()` is
// false.
TEST_F(UnifiedSystemTrayAccessibilityTest,
       NameWithBatterySaverEnabledNotCharging) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kBatterySaver);

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  power_manager::PowerSupplyProperties prop;
  // For the logic we're testing, the `PowerSupplyProperties_BatteryState`
  // cannot be FULL or CHARGING so we'll just use DISCHARGING.
  prop.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);

  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetProtoForTesting(prop);

  // First, test the name is set correctly when `IsBatterySaveryActive()`
  // returns true.
  fake_power_status->SetIsBatterySaverActive(true);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly.
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(&status,
                     FormatPowerPercentageString(
                         IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_BSM_ON_ACCESSIBLE,
                         fake_power_status),
                     StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }

  // Second, test the name is set correctly when `IsBatterySaveryActive()`
  // returns false.
  fake_power_status->SetIsBatterySaverActive(false);

  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE, fake_power_status),
      StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }
}

// This tests the logic in `PowerStatus::GetAccessibleNameString` where
// `features::IsBatterySaverAvailable()` is false.
TEST_F(UnifiedSystemTrayAccessibilityTest, NameWithBatterySaverDisabled) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  // First, test the name is set correctly when `IsBatteryCharging()`
  // returns true.
  power_manager::PowerSupplyProperties prop;
  prop.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetProtoForTesting(prop);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly.
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE,
          fake_power_status),
      StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }

  // Second, test the name is set correctly when `IsBatteryCharging()`
  // returns false. For the purposes of this test, the battery state cannot be
  // FULL or CHARGING so we will use DISCHARGING.
  prop.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  // Pass in the proto again to get the updated value for the battery state.
  fake_power_status->SetProtoForTesting(prop);
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE, fake_power_status),
      StatusType::kBattery);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }
}

// This tests the logic in `PowerStatus::GetAccessibleNameString` where
// `features::IsBatteryChargeLimitAvailable()` and `IsBatteryChargeLimited()`
// are both true.
TEST_F(UnifiedSystemTrayAccessibilityTest, NameWithBatteryChargeLimitEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kBatteryChargeLimit);

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  power_manager::PowerSupplyProperties prop;
  prop.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  prop.set_charge_limited(true);

  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetProtoForTesting(prop);
  fake_power_status->SetBatteryPercent(80.0);

  // `OnPowerStatusChanged` is called in an asynchronous method, but for the
  // purpose of this test, it is called explicitly.
  GetPrimaryUnifiedSystemTray()->OnPowerStatusChanged();

  // The new logic should return just the percentage accessible string, not a
  // full description with time, etc.
  UpdatePartOfStatus(
      &status,
      FormatPowerPercentageString(
          IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ON_HOLD_ACCESSIBLE,
          fake_power_status),
      StatusType::kBattery);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

TEST_F(UnifiedSystemTrayAccessibilityTest, ChannelIndicatorUpdatesName) {
  ASSERT_TRUE(GetPrimaryUnifiedSystemTray()->ShouldChannelIndicatorBeShown());
  ASSERT_TRUE(channel_indicator_view());

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  channel_indicator_view()->SetVisible(true);
  ASSERT_TRUE(channel_indicator_view()->GetVisible());

  // First, test the accessible name string if the image_view is visible.
  EXPECT_TRUE(channel_indicator_view()->IsImageViewVisibleForTesting());
  UpdatePartOfStatus(&status,
                     channel_indicator_view()
                         ->image_view()
                         ->GetViewAccessibility()
                         .GetCachedName(),
                     StatusType::kChannelIndicator);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }

  // Next, test the accessible name string if the label_view is visible.
  // Changing the session state from ACTIVE to another state will force the
  // image_view to be destroyed and the label_view to be created.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  // Updating the session state triggers a string change in the
  // `network_tray_view()`. For the purposes of this test, we want to isolate
  // the `channel_indicator_view()`, so set the `network_tray_view()` invisible
  // for now so its portion of the tray's accessible name remains empty string.
  network_tray_view()->SetVisible(false);
  EXPECT_FALSE(channel_indicator_view()->IsImageViewVisibleForTesting());
  EXPECT_TRUE(channel_indicator_view()->IsLabelVisibleForTesting());
  EXPECT_FALSE(network_tray_view()->GetVisible());
  UpdatePartOfStatus(
      &status,
      channel_indicator_view()->label()->GetViewAccessibility().GetCachedName(),
      StatusType::kChannelIndicator);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }
}

// There are already NetworkTrayViewTests testing the complex logic used to
// calculate the `network_tray_view()` tooltip. For UnifiedSystemTray, we just
// want to make sure that the tray's accessible name updates when the
// `network_tray_view()`'s tooltip changes.
TEST_F(UnifiedSystemTrayAccessibilityTest, NetworkTrayUpdatesName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  network_tray_view()->SetVisible(true);
  EXPECT_TRUE(network_tray_view()->GetVisible());

  // The network will start out disconnected.
  UpdatePartOfStatus(&status,
                     l10n_util::GetStringUTF16(
                         IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_TOOLTIP),
                     StatusType::kNetworkHotspot);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }

  network_tray_view()->SetTooltipText(u"Test network tooltip");
  UpdatePartOfStatus(&status, u"Test network tooltip",
                     StatusType::kNetworkHotspot);

  {
    ui::AXNodeData data;
    GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
        &data);
    EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
  }
}

// There are already HotspotTrayViewTests testing logic used to calculate the
// `hotspot_tray_view()` tooltip. For UnifiedSystemTray, we just want to make
// sure that the tray's accessible name updates when the `hotspot_tray_view()`'s
// tooltip changes.
TEST_F(UnifiedSystemTrayAccessibilityTest, HotspotTrayUpdatesName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  hotspot_tray_view()->SetVisible(true);
  EXPECT_TRUE(hotspot_tray_view()->GetVisible());
  hotspot_tray_view()->SetTooltipText(u"Test hotspot tooltip");
  UpdatePartOfStatus(&status, u"Test hotspot tooltip",
                     StatusType::kNetworkHotspot);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

// Test that both the `hotspot_tray_view()` and `network_tray_view()` tooltips
// are present in the UnifiedSystemTray's name if both are visible.
TEST_F(UnifiedSystemTrayAccessibilityTest, HotspotAndNetworkCombinedInName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  hotspot_tray_view()->SetVisible(true);
  EXPECT_TRUE(hotspot_tray_view()->GetVisible());
  std::u16string hotspot_string = u"Test hotspot tooltip";
  hotspot_tray_view()->SetTooltipText(hotspot_string);

  network_tray_view()->SetVisible(true);
  EXPECT_TRUE(network_tray_view()->GetVisible());
  std::u16string network_string = u"Test network tooltip";
  network_tray_view()->SetTooltipText(network_string);

  UpdatePartOfStatus(&status,
                     l10n_util::GetStringFUTF16(
                         IDS_ASH_STATUS_TRAY_NETWORK_ACCESSIBLE_DESCRIPTION,
                         {hotspot_string, network_string}, /*offsets=*/nullptr),
                     StatusType::kNetworkHotspot);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

// This test follows the logic in
// `ManagedDeviceTrayItemView::UpdateTooltipText()` when
// `session_>IsUserPublicAccount()` returns true.
TEST_F(UnifiedSystemTrayAccessibilityTest,
       ManagedDevicePublicSessionUpdatesName) {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  EnterpriseDomainModel* enterprise_domain_model =
      Shell::Get()->system_tray_model()->enterprise_domain();
  RegisterUserWithUserPrefs(AccountId::FromUserEmail(user_email),
                            user_manager::UserType::kPublicAccount);
  EXPECT_TRUE(session_controller->IsActiveUserSessionStarted());
  EXPECT_TRUE(session_controller->IsUserPublicAccount());
  // Simulate enterprise information becoming available.
  enterprise_domain_model->SetDeviceEnterpriseInfo(DeviceEnterpriseInfo{
      "example.com", ManagementDeviceMode::kChromeEnterprise});
  EXPECT_FALSE(enterprise_domain_model->enterprise_domain_manager().empty());

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  UpdatePartOfStatus(
      &status,
      l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              enterprise_domain_model->enterprise_domain_manager())),
      StatusType::kManagedDevice);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

// This test follows the logic in
// `ManagedDeviceTrayItemView::UpdateTooltipText()` when
// `session_>IsUserChild()` returns true.
TEST_F(UnifiedSystemTrayAccessibilityTest,
       ManagedDeviceChildSessionUpdatesName) {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  RegisterUserWithUserPrefs(AccountId::FromUserEmail(user_email),
                            user_manager::UserType::kChild);
  EXPECT_TRUE(session_controller->IsActiveUserSessionStarted());
  EXPECT_FALSE(session_controller->IsUserPublicAccount());
  EXPECT_TRUE(session_controller->IsUserChild());

  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  UpdatePartOfStatus(
      &status, l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FAMILY_LINK_LABEL),
      StatusType::kManagedDevice);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

TEST_F(UnifiedSystemTrayAccessibilityTest, ImeModeUpdatesName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  Shell::Get()->ime_controller()->SetImesManagedByPolicy(true);
  EXPECT_TRUE(ime_mode_view()->GetVisible());
  std::vector<ImeInfo> imes;
  ImeInfo test_ime;
  test_ime.id = "test id";
  test_ime.name = u"test keyboard";
  imes.push_back(std::move(test_ime));
  Shell::Get()->ime_controller()->RefreshIme("test id", std::move(imes),
                                             std::vector<ImeMenuItem>());

  UpdatePartOfStatus(
      &status,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_INDICATOR_IME_TOOLTIP,
                                 u"test keyboard"),
      StatusType::kImeMode);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}

TEST_F(UnifiedSystemTrayAccessibilityTest, CurrentLocaleUpdatesName) {
  std::vector<std::u16string> status;
  CreateDefaultStatusForTesting(&status);

  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back("en-US", u"English (United States)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   "en-US");
  EXPECT_TRUE(current_locale_view()->GetVisible());

  UpdatePartOfStatus(
      &status,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_INDICATOR_LOCALE_TOOLTIP,
                                 u"English (United States)"),
      StatusType::kCurrentLocale);

  ui::AXNodeData data;
  GetPrimaryUnifiedSystemTray()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION, status, nullptr));
}
}  // namespace ash
