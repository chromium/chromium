// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/feature_tiles_container_view.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/message_center.h"

namespace ash {

using message_center::MessageCenter;
using message_center::Notification;

// `CastConfigController` must be overridden so a `cast_config_` object exists.
// This is required to make the cast tile visible in the
// `CastAndAutoRotateCompactTiles` unit test. Cast features will not be used.
class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController() = default;
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override = default;

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override {
    return has_media_router_;
  }
  bool HasSinksAndRoutes() const override { return has_sinks_and_routes_; }
  bool HasActiveRoute() const override { return false; }
  bool AccessCodeCastingEnabled() const override {
    return access_code_casting_enabled_;
  }
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override {
    return sinks_and_routes_;
  }
  void CastToSink(const std::string& sink_id) override {}
  void StopCasting(const std::string& route_id) override {}

  bool has_media_router_ = true;
  bool has_sinks_and_routes_ = false;
  bool access_code_casting_enabled_ = false;
  std::vector<SinkAndRoute> sinks_and_routes_;
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
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    }
    AshTestBase::SetUp();
    cast_config_ = std::make_unique<TestCastConfigController>();
  }

  void TearDown() override {
    cast_config_.reset();
    AshTestBase::TearDown();
  }

  bool IsQsRevampEnabled() { return GetParam(); }

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
  // one notification in the notification list. This should only be called when
  // QsRevamp is enabled.
  void ShowNotificationBubble() {
    DCHECK(IsQsRevampEnabled());
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->shelf()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->ShowBubble();
  }

  // Hide the notification center bubble. This assumes that it is already shown.
  // This should only be called when QsRevamp is enabled.
  void HideNotificationBubble() {
    DCHECK(IsQsRevampEnabled());
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
    auto* bubble =
        GetPrimaryUnifiedSystemTray()->slider_bubble_controller_->bubble_view_;
    return bubble ? bubble->GetBoundsInScreen() : gfx::Rect();
  }

  FeatureTile* GetTileById(int tile_view_id) {
    views::View* tile_view = GetPrimaryUnifiedSystemTray()
                                 ->bubble()
                                 ->quick_settings_view()
                                 ->GetViewByID(tile_view_id);
    return static_cast<FeatureTile*>(tile_view);
  }

  TimeTrayItemView* time_view() {
    return GetPrimaryUnifiedSystemTray()->time_view_;
  }

  ImeModeView* ime_mode_view() {
    return GetPrimaryUnifiedSystemTray()->ime_mode_view_;
  }

 private:
  int id_ = 0;
  std::unique_ptr<TestCastConfigController> cast_config_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedSystemTrayTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

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
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
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
                              ->horizontal_label_for_test()
                              ->GetBoundsInScreen();
  gfx::Rect ime_bounds = ime_mode_view()->label()->GetBoundsInScreen();

  EXPECT_EQ(time_bounds.y(), ime_bounds.y());
  EXPECT_EQ(time_bounds.height(), ime_bounds.height());
}

TEST_P(UnifiedSystemTrayTest, FocusMessageCenter) {
  if (IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  auto* message_center_view =
      tray->message_center_bubble()->notification_center_view();
  auto* focus_manager = message_center_view->GetFocusManager();

  AddNotification();
  AddNotification();
  message_center_view->SetVisible(true);

  EXPECT_FALSE(message_center_view->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(message_center_view->collapsed());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_TRUE(did_focus);

  EXPECT_TRUE(tray->IsMessageCenterBubbleShown());
  EXPECT_FALSE(message_center_view->collapsed());
  EXPECT_TRUE(message_center_view->Contains(focus_manager->GetFocusedView()));
}

TEST_P(UnifiedSystemTrayTest, FocusMessageCenter_MessageCenterBubbleNotShown) {
  if (IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* message_center_bubble = tray->message_center_bubble();

  EXPECT_FALSE(message_center_bubble->IsMessageCenterVisible());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_FALSE(did_focus);
}

TEST_P(UnifiedSystemTrayTest, FocusMessageCenter_VoxEnabled) {
  if (IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  auto* message_center_bubble = tray->message_center_bubble();
  auto* message_center_view = message_center_bubble->notification_center_view();

  AddNotification();
  AddNotification();
  message_center_view->SetVisible(true);
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  EXPECT_FALSE(message_center_bubble->GetBubbleWidget()->IsActive());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_TRUE(did_focus);

  auto* focus_manager = tray->GetFocusManager();

  EXPECT_TRUE(tray->IsMessageCenterBubbleShown());
  EXPECT_TRUE(message_center_bubble->GetBubbleWidget()->IsActive());
  EXPECT_FALSE(message_center_view->Contains(focus_manager->GetFocusedView()));
}

TEST_P(UnifiedSystemTrayTest, FocusQuickSettings) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  if (IsQsRevampEnabled()) {
    auto* quick_settings_view = tray->bubble()->quick_settings_view();
    auto* focus_manager = quick_settings_view->GetFocusManager();
    EXPECT_FALSE(
        quick_settings_view->Contains(focus_manager->GetFocusedView()));

    // There's no `FocusQuickSettings` method in the new view. Press the tab key
    // should focus on the first button in the qs bubble.
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
    EXPECT_TRUE(quick_settings_view->Contains(focus_manager->GetFocusedView()));
    return;
  }

  auto* unified_system_tray_view = tray->bubble()->unified_view();
  auto* focus_manager = unified_system_tray_view->GetFocusManager();

  EXPECT_FALSE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_TRUE(did_focus);

  EXPECT_TRUE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));
}

TEST_P(UnifiedSystemTrayTest, FocusQuickSettings_BubbleNotShown) {
  auto* tray = GetPrimaryUnifiedSystemTray();

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_FALSE(did_focus);
}

TEST_P(UnifiedSystemTrayTest, FocusQuickSettings_VoxEnabled) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* tray_bubble_widget = tray->bubble()->GetBubbleWidget();

  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  EXPECT_FALSE(tray_bubble_widget->IsActive());

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_TRUE(did_focus);

  if (IsQsRevampEnabled()) {
    auto* quick_settings_view = tray->bubble()->quick_settings_view();
    auto* focus_manager = quick_settings_view->GetFocusManager();
    EXPECT_TRUE(tray_bubble_widget->IsActive());
    EXPECT_FALSE(
        quick_settings_view->Contains(focus_manager->GetFocusedView()));
    return;
  }

  auto* unified_system_tray_view = tray->bubble()->unified_view();
  auto* focus_manager = unified_system_tray_view->GetFocusManager();

  EXPECT_TRUE(tray_bubble_widget->IsActive());
  EXPECT_FALSE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));
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

// Tests that CalendarView switches back to Quick Settings when screen size is
// limited and the bubble requires a collapsed state.
TEST_P(UnifiedSystemTrayTest, CalendarGoesToMainView) {
  if (IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  // Set a limited screen size.
  UpdateDisplay("800x600");

  // Generate a notification, close and open the bubble so we can show the
  // collapsed message center.
  AddNotification();
  tray->CloseBubble();
  tray->ShowBubble();

  // Ensure message center is collapsed when Calendar is not being shown.
  auto* message_center_view =
      tray->message_center_bubble()->notification_center_view();
  EXPECT_FALSE(tray->IsShowingCalendarView());
  EXPECT_TRUE(message_center_view->collapsed());

  // Ensure message center is collapsed when the Calendar is being shown.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(tray->IsShowingCalendarView());
  EXPECT_TRUE(message_center_view->collapsed());

  // Test that Calendar is no longer shown after expanding the collapsed
  // message center.
  tray->message_center_bubble()->ExpandMessageCenter();
  EXPECT_FALSE(message_center_view->collapsed());
  EXPECT_FALSE(tray->IsShowingCalendarView());
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

// Tests that the bubble is closed after entering or exiting tablet mode.
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
            ShelfConfig::Get()->GetShelfControlButtonColor(widget));

  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_EQ(tray->layer()->background_color(),
            ShelfConfig::Get()->GetShelfControlButtonColor(widget));
}

// Tests that the bubble automatically hides if it is visible when another
// bubble becomes visible, and otherwise does not automatically show or hide.
TEST_P(UnifiedSystemTrayTest, BubbleHideBehavior) {
  // This hiding behavior only applies when QsRevamp is enabled.
  if (!IsQsRevampEnabled()) {
    return;
  }

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
  EXPECT_GT(464, bubble_view->height());

  // Goes to a detailed view (here using calendar view).
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Asserts that calendar is actually shown.
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());

  if (IsQsRevampEnabled()) {
    // The bubble height should be fixed to the detailed view height.
    EXPECT_EQ(464, bubble_view->height());
  } else {
    EXPECT_GT(464, bubble_view->height());
  }
  tray->CloseBubble();
}

TEST_P(UnifiedSystemTrayTest, BubbleViewSizeChangeNoEnoughSpace) {
  // Set a small screen size.
  UpdateDisplay("300x200");

  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* bubble_view = tray->bubble()->GetBubbleView();

  // The main page height should be smaller than the detailed view height.
  EXPECT_GT(464, bubble_view->height());

  // Goes to a detailed view (here using calendar view).
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  // Asserts that calendar is actually shown.
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());

  // No enough space for the fixed detailed view height.
  EXPECT_GT(464, bubble_view->height());
  tray->CloseBubble();
}

// Tests that the cast and auto-rotate tiles are presented in their compact
// version when they are both visible.
TEST_P(UnifiedSystemTrayTest, CastAndAutoRotateCompactTiles) {
  // Feature tiles only exist when QsRevamp is enabled.
  if (!IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // Test that the cast tile is in its primary form when in clamshell mode,
  // when the auto-rotate tile is not visible.
  EXPECT_FALSE(tablet_mode_controller->IsInTabletMode());
  tray->ShowBubble();

  FeatureTile* cast_tile = GetTileById(VIEW_ID_CAST_MAIN_VIEW);
  ASSERT_TRUE(cast_tile);
  EXPECT_TRUE(cast_tile->GetVisible());
  EXPECT_EQ(cast_tile->tile_type(), FeatureTile::TileType::kPrimary);

  FeatureTile* autorotate_tile = GetTileById(VIEW_ID_AUTOROTATE_FEATURE_TILE);
  EXPECT_FALSE(autorotate_tile->GetVisible());

  tray->CloseBubble();

  // Test that cast and auto-rotate tiles are compact in tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(tablet_mode_controller->IsInTabletMode());

  tray->ShowBubble();

  cast_tile = GetTileById(VIEW_ID_CAST_MAIN_VIEW);
  EXPECT_TRUE(cast_tile->GetVisible());
  EXPECT_EQ(cast_tile->tile_type(), FeatureTile::TileType::kCompact);

  autorotate_tile = GetTileById(VIEW_ID_AUTOROTATE_FEATURE_TILE);
  EXPECT_TRUE(autorotate_tile->GetVisible());
  EXPECT_EQ(autorotate_tile->tile_type(), FeatureTile::TileType::kCompact);

  tray->CloseBubble();
}

// Tests that the screen capture and DND tiles are presented in their compact
// version when they are both visible.
TEST_P(UnifiedSystemTrayTest, CaptureAndDNDCompactTiles) {
  // Feature tiles only exist when QsRevamp is enabled.
  if (!IsQsRevampEnabled()) {
    return;
  }

  auto* tray = GetPrimaryUnifiedSystemTray();

  tray->ShowBubble();

  FeatureTile* capture_tile = GetTileById(VIEW_ID_SCREEN_CAPTURE_FEATURE_TILE);
  EXPECT_TRUE(capture_tile->GetVisible());
  EXPECT_EQ(capture_tile->tile_type(), FeatureTile::TileType::kCompact);

  FeatureTile* dnd_tile = GetTileById(VIEW_ID_DND_FEATURE_TILE);
  EXPECT_TRUE(dnd_tile->GetVisible());
  EXPECT_EQ(dnd_tile->tile_type(), FeatureTile::TileType::kCompact);

  tray->CloseBubble();

  // TODO(b/266000781): Add test cases for when one tile is visible but the
  // other is not, to test they show in their primary forms.
}

}  // namespace ash
