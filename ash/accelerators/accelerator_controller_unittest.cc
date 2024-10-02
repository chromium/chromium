// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <optional>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_history_impl.h"
#include "ash/accelerators/accelerator_notifications.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/pre_target_accelerator_handler.h"
#include "ash/accelerators/system_shortcut_behavior_policy.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_media_client.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/test/test_session_state_animator.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/media_keys_util.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_targeter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;
using ::media_session::mojom::MediaSessionAction;

constexpr char kUserEmail[] = "user@testemail.com";

struct PrefToAcceleratorEntry {
  const char* pref_name;
  // If |notification_id| has been set to nullptr, then no notification is
  // expected.
  const char* notification_id;
  const ui::Accelerator accelerator;
};

constexpr char kCapsLockNoMatchNudgeId[] = "caps-lock-no-match-nudge-id";

const PrefToAcceleratorEntry kAccessibilityAcceleratorMap[] = {
    {
        prefs::kAccessibilityHighContrastEnabled,
        kHighContrastToggleAccelNotificationId,
        ui::Accelerator(ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
    },
    {prefs::kDockedMagnifierEnabled, kDockedMagnifierToggleAccelNotificationId,
     ui::Accelerator(ui::VKEY_D, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)},
    {
        prefs::kAccessibilitySpokenFeedbackEnabled,
        nullptr,
        ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
    },
    {prefs::kAccessibilityScreenMagnifierEnabled,
     kFullscreenMagnifierToggleAccelNotificationId,
     ui::Accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)},
};

void AddTestImes() {
  ImeInfo ime1;
  ime1.id = "id1";
  ImeInfo ime2;
  ime2.id = "id2";
  std::vector<ImeInfo> visible_imes;
  visible_imes.push_back(std::move(ime1));
  visible_imes.push_back(std::move(ime2));
  Shell::Get()->ime_controller()->RefreshIme("id1", std::move(visible_imes),
                                             std::vector<ImeMenuItem>());
}

void AddNotVisibleTestIme() {
  ImeInfo dictation;
  dictation.id = "_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation";
  const std::vector<ImeInfo> visible_imes =
      Shell::Get()->ime_controller()->GetVisibleImes();
  std::vector<ImeInfo> available_imes;
  for (auto ime : visible_imes) {
    available_imes.push_back(ime);
  }
  available_imes.push_back(dictation);
  Shell::Get()->ime_controller()->RefreshIme(
      dictation.id, std::move(available_imes), std::vector<ImeMenuItem>());
}

ui::Accelerator CreateReleaseAccelerator(ui::KeyboardCode key_code,
                                         int modifiers) {
  ui::Accelerator accelerator(key_code, modifiers);
  accelerator.set_key_state(ui::Accelerator::KeyState::RELEASED);
  return accelerator;
}

class DummyBrightnessControlDelegate : public BrightnessControlDelegate {
 public:
  DummyBrightnessControlDelegate()
      : handle_brightness_down_count_(0), handle_brightness_up_count_(0) {}

  DummyBrightnessControlDelegate(const DummyBrightnessControlDelegate&) =
      delete;
  DummyBrightnessControlDelegate& operator=(
      const DummyBrightnessControlDelegate&) = delete;

  ~DummyBrightnessControlDelegate() override = default;

  void HandleBrightnessDown() override {
    ++handle_brightness_down_count_;
    last_accelerator_ = ui::Accelerator(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  }
  void HandleBrightnessUp() override {
    ++handle_brightness_up_count_;
    last_accelerator_ = ui::Accelerator(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  }
  void SetBrightnessPercent(double percent,
                            bool gradual,
                            BrightnessChangeSource source) override {}
  void GetBrightnessPercent(
      base::OnceCallback<void(std::optional<double>)> callback) override {
    std::move(callback).Run(100.0);
  }
  void SetAmbientLightSensorEnabled(
      bool enabled,
      AmbientLightSensorEnabledChangeSource source) override {}
  void GetAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(true);
  }
  void HasAmbientLightSensor(
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(true);
  }

  int handle_brightness_down_count() const {
    return handle_brightness_down_count_;
  }
  int handle_brightness_up_count() const { return handle_brightness_up_count_; }
  const ui::Accelerator& last_accelerator() const { return last_accelerator_; }

 private:
  int handle_brightness_down_count_;
  int handle_brightness_up_count_;
  ui::Accelerator last_accelerator_;
};

class DummyKeyboardBrightnessControlDelegate
    : public KeyboardBrightnessControlDelegate {
 public:
  DummyKeyboardBrightnessControlDelegate() = default;

  DummyKeyboardBrightnessControlDelegate(
      const DummyKeyboardBrightnessControlDelegate&) = delete;
  DummyKeyboardBrightnessControlDelegate& operator=(
      const DummyKeyboardBrightnessControlDelegate&) = delete;

  ~DummyKeyboardBrightnessControlDelegate() override = default;

  void HandleKeyboardBrightnessDown() override {
    ++handle_keyboard_brightness_down_count_;
    last_accelerator_ =
        ui::Accelerator(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN);
  }

  void HandleKeyboardBrightnessUp() override {
    ++handle_keyboard_brightness_up_count_;
    last_accelerator_ =
        ui::Accelerator(ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN);
  }

  void HandleToggleKeyboardBacklight() override {
    ++handle_toggle_keyboard_backlight_count_;
    last_accelerator_ =
        ui::Accelerator(ui::VKEY_KBD_BACKLIGHT_TOGGLE, ui::EF_NONE);
  }

  void HandleSetKeyboardBrightness(
      double percent,
      bool gradual,
      KeyboardBrightnessChangeSource source) override {}

  void HandleGetKeyboardBrightness(
      base::OnceCallback<void(std::optional<double>)> callback) override {
    std::move(callback).Run(100.0);
  }

  void HandleSetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      KeyboardAmbientLightSensorEnabledChangeSource source) override {}

  void HandleGetKeyboardAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override {}

  int handle_keyboard_brightness_down_count() const {
    return handle_keyboard_brightness_down_count_;
  }

  int handle_keyboard_brightness_up_count() const {
    return handle_keyboard_brightness_up_count_;
  }

  int handle_toggle_keyboard_backlight_count() const {
    return handle_toggle_keyboard_backlight_count_;
  }

  const ui::Accelerator& last_accelerator() const { return last_accelerator_; }

 private:
  int handle_keyboard_brightness_down_count_ = 0;
  int handle_keyboard_brightness_up_count_ = 0;
  int handle_toggle_keyboard_backlight_count_ = 0;
  ui::Accelerator last_accelerator_;
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void, OpenCalculator, (), (override));
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class MockAcceleratorObserver
    : public testing::NiceMock<AcceleratorController::Observer> {
 public:
  // AcceleratorController::Observer:
  MOCK_METHOD(void, OnActionPerformed, (AcceleratorAction action), (override));
  MOCK_METHOD(void,
              OnAcceleratorControllerWillBeDestroyed,
              (AcceleratorController * controller),
              (override));
};

class VoidEventHandler : public ui::EventHandler {
 public:
  VoidEventHandler() = default;

  void OnEvent(ui::Event* event) override {
    num_events_received_++;
    event->SetHandled();
    event->StopPropagation();
  }

  void ResetEventCounter() { num_events_received_ = 0; }
  int num_events_received() const { return num_events_received_; }

 private:
  int num_events_received_ = 0;
};

}  // namespace

// Note AcceleratorControllerTest can't be in the anonymous namespace because
// it is referenced as a friend by exit_warning_handler.h
class AcceleratorControllerTest : public AshTestBase {
 public:
  AcceleratorControllerTest() = default;

  AcceleratorControllerTest(const AcceleratorControllerTest&) = delete;
  AcceleratorControllerTest& operator=(const AcceleratorControllerTest&) =
      delete;

  ~AcceleratorControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->accelerator_controller();
    test_api_ =
        std::make_unique<AcceleratorControllerImpl::TestApi>(controller_);
  }

 protected:
  static bool ProcessInController(const ui::Accelerator& accelerator) {
    AcceleratorControllerImpl* controller =
        Shell::Get()->accelerator_controller();
    if (accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
      // If the |accelerator| should trigger on release, then we store the
      // pressed version of it first in history then the released one to
      // simulate what happens in reality.
      ui::Accelerator pressed_accelerator = accelerator;
      pressed_accelerator.set_key_state(ui::Accelerator::KeyState::PRESSED);
      controller->GetAcceleratorHistory()->StoreCurrentAccelerator(
          pressed_accelerator);
    }
    controller->GetAcceleratorHistory()->StoreCurrentAccelerator(accelerator);
    return controller->Process(accelerator);
  }

  bool ContainsHighContrastNotification() const {
    return nullptr != message_center()->FindVisibleNotificationById(
                          kHighContrastToggleAccelNotificationId);
  }

  bool ContainsDockedMagnifierNotification() const {
    return nullptr != message_center()->FindVisibleNotificationById(
                          kDockedMagnifierToggleAccelNotificationId);
  }

  bool ContainsFullscreenMagnifierNotification() const {
    return nullptr != message_center()->FindVisibleNotificationById(
                          kFullscreenMagnifierToggleAccelNotificationId);
  }

  bool IsNotificationPinned(const std::string& id) const {
    auto* notification = message_center()->FindVisibleNotificationById(id);
    DCHECK(notification);
    return notification->pinned() ||
           notification->priority() == message_center::SYSTEM_PRIORITY;
  }

  AccessibilityConfirmationDialog* GetConfirmationDialog() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetConfirmationDialogForTest();
  }

  bool IsConfirmationDialogOpen() { return !!GetConfirmationDialog(); }

  void AcceptConfirmationDialog() {
    DCHECK(GetConfirmationDialog());
    GetConfirmationDialog()->AcceptDialog();
  }

  void CancelConfirmationDialog() {
    DCHECK(GetConfirmationDialog());
    GetConfirmationDialog()->CancelDialog();
  }

  void TriggerRotateScreenShortcut() {
    PressAndReleaseKey(ui::VKEY_BROWSER_REFRESH,
                       ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    if (IsConfirmationDialogOpen()) {
      AcceptConfirmationDialog();
      base::RunLoop().RunUntilIdle();
    }
  }

  void RemoveAllNotifications() const {
    message_center()->RemoveAllNotifications(
        false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
  }

  static const ui::Accelerator& GetPreviousAccelerator() {
    return Shell::Get()
        ->accelerator_controller()
        ->GetAcceleratorHistory()
        ->previous_accelerator();
  }

  static const ui::Accelerator& GetCurrentAccelerator() {
    return Shell::Get()
        ->accelerator_controller()
        ->GetAcceleratorHistory()
        ->current_accelerator();
  }

  // Several functions to access ExitWarningHandler (as friend).
  static void StubForTest(ExitWarningHandler* ewh) {
    ewh->stub_timer_for_test_ = true;
  }
  static void Reset(ExitWarningHandler* ewh) {
    ewh->state_ = ExitWarningHandler::IDLE;
  }
  static void SimulateTimerExpired(ExitWarningHandler* ewh) {
    ewh->TimerAction();
  }
  static bool is_ui_shown(ExitWarningHandler* ewh) { return !!ewh->widget_; }
  // Adding a test API in test fixture to extract the view.
  static views::View* GetContentsView(ExitWarningHandler* ewh) {
    return ewh->widget_->GetContentsView();
  }
  static bool is_idle(ExitWarningHandler* ewh) {
    return ewh->state_ == ExitWarningHandler::IDLE;
  }
  static bool is_exiting(ExitWarningHandler* ewh) {
    return ewh->state_ == ExitWarningHandler::EXITING;
  }

  message_center::MessageCenter* message_center() const {
    return message_center::MessageCenter::Get();
  }

  void SetBrightnessControlDelegate(
      std::unique_ptr<BrightnessControlDelegate> delegate) {
    Shell::Get()->brightness_control_delegate_ = std::move(delegate);
  }

  void SetKeyboardBrightnessControlDelegate(
      std::unique_ptr<KeyboardBrightnessControlDelegate> delegate) {
    Shell::Get()->keyboard_brightness_control_delegate_ = std::move(delegate);
  }

  bool WriteJsonFile(const base::FilePath& file_path,
                     const std::string& json_string) const {
    if (!base::DirectoryExists(file_path.DirName()))
      base::CreateDirectory(file_path.DirName());

    if (!base::WriteFile(file_path, json_string)) {
      LOG(ERROR) << "Writing to " << file_path.value() << " failed.";
      return false;
    }
    return true;
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  raw_ptr<AcceleratorControllerImpl, DanglingUntriaged> controller_ =
      nullptr;  // Not owned.
  std::unique_ptr<AcceleratorControllerImpl::TestApi> test_api_;

 private:
  MockNewWindowDelegate new_window_delegate_;
};

namespace {

// Double press of exit shortcut => exiting
TEST_F(AcceleratorControllerTest, ExitWarningHandlerTestDoublePress) {
  ui::Accelerator press(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  ui::Accelerator release(press);
  release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ExitWarningHandler* ewh = test_api_->GetExitWarningHandler();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));
  EXPECT_FALSE(ProcessInController(release));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));  // second press before timer.
  EXPECT_FALSE(ProcessInController(release));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_exiting(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);
}

// Single press of exit shortcut before timer => idle
TEST_F(AcceleratorControllerTest, ExitWarningHandlerTestSinglePress) {
  ui::Accelerator press(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  ui::Accelerator release(press);
  release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ExitWarningHandler* ewh = test_api_->GetExitWarningHandler();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));
  EXPECT_FALSE(ProcessInController(release));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);
}

// Shutdown ash with exit warning bubble open should not crash.
TEST_F(AcceleratorControllerTest, LingeringExitWarningBubble) {
  ExitWarningHandler* ewh = test_api_->GetExitWarningHandler();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);

  // Trigger once to show the bubble.
  ewh->HandleAccelerator();
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));

  // Exit ash and there should be no crash
}

TEST_F(AcceleratorControllerTest,
       ExitWarningWidgetDelegateViewAccessibleProperties) {
  ExitWarningHandler* ewh = test_api_->GetExitWarningHandler();
  ewh->HandleAccelerator();
  auto* delegate_view = GetContentsView(ewh);
  ui::AXNodeData data;

  delegate_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kAlert);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_SIGN_OUT_WARNING_POPUP_TEXT_ACCESSIBLE));
}

TEST_F(AcceleratorControllerTest, Register) {
  ui::TestAcceleratorTarget target;
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  const ui::Accelerator accelerator_c(ui::VKEY_C, ui::EF_NONE);
  const ui::Accelerator accelerator_d(ui::VKEY_D, ui::EF_NONE);

  controller_->Register(
      {accelerator_a, accelerator_b, accelerator_c, accelerator_d}, &target);

  // The registered accelerators are processed.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_TRUE(ProcessInController(accelerator_b));
  EXPECT_TRUE(ProcessInController(accelerator_c));
  EXPECT_TRUE(ProcessInController(accelerator_d));
  EXPECT_EQ(4, target.accelerator_count());
}

TEST_F(AcceleratorControllerTest, RegisterMultipleTarget) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  ui::TestAcceleratorTarget target1;
  controller_->Register({accelerator_a}, &target1);
  ui::TestAcceleratorTarget target2;
  controller_->Register({accelerator_a}, &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_count());
  EXPECT_EQ(1, target2.accelerator_count());
}

TEST_F(AcceleratorControllerTest, Unregister) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  ui::TestAcceleratorTarget target;
  controller_->Register({accelerator_a, accelerator_b}, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  controller_->Unregister(accelerator_b, &target);
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(1, target.accelerator_count());

  // The unregistered accelerator is no longer processed.
  target.ResetCounts();
  controller_->Unregister(accelerator_a, &target);
  EXPECT_FALSE(ProcessInController(accelerator_a));
  EXPECT_EQ(0, target.accelerator_count());
}

TEST_F(AcceleratorControllerTest, UnregisterAll) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  ui::TestAcceleratorTarget target1;
  controller_->Register({accelerator_a, accelerator_b}, &target1);
  const ui::Accelerator accelerator_c(ui::VKEY_C, ui::EF_NONE);
  ui::TestAcceleratorTarget target2;
  controller_->Register({accelerator_c}, &target2);
  controller_->UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(ProcessInController(accelerator_a));
  EXPECT_FALSE(ProcessInController(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(ProcessInController(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_count());
}

TEST_F(AcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  ui::TestAcceleratorTarget target1;
  controller_->Register({accelerator_a}, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(1, target1.accelerator_count());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(ProcessInController(accelerator_b));
}

TEST_F(AcceleratorControllerTest, IsRegistered) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_shift_a(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  ui::TestAcceleratorTarget target;
  controller_->Register({accelerator_a}, &target);
  EXPECT_TRUE(controller_->IsRegistered(accelerator_a));
  EXPECT_FALSE(controller_->IsRegistered(accelerator_shift_a));
  controller_->UnregisterAll(&target);
  EXPECT_FALSE(controller_->IsRegistered(accelerator_a));
}

TEST_F(AcceleratorControllerTest, WindowSnap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state = WindowState::Get(window.get());

  window_state->Activate();

  {
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                        {});
    gfx::Rect expected_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kPrimary);
    EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
  }
  {
    controller_->PerformActionIfEnabled(
        AcceleratorAction::kWindowCycleSnapRight, {});
    gfx::Rect expected_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kSecondary);
    EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
  }
  {
    gfx::Rect normal_bounds = window_state->GetRestoreBoundsInParent();

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleMaximized,
                                        {});
    EXPECT_TRUE(window_state->IsMaximized());
    EXPECT_NE(normal_bounds.ToString(), window->bounds().ToString());

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleMaximized,
                                        {});
    EXPECT_FALSE(window_state->IsMaximized());

    // Window gets restored to its right snapped window bounds and its window
    // state should also restore to the right snapped window state.
    gfx::Rect expected_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kSecondary);
    EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
    EXPECT_EQ(window_state->GetStateType(), WindowStateType::kSecondarySnapped);

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleMaximized,
                                        {});
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                        {});
    EXPECT_FALSE(window_state->IsMaximized());

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleMaximized,
                                        {});
    controller_->PerformActionIfEnabled(
        AcceleratorAction::kWindowCycleSnapRight, {});
    EXPECT_FALSE(window_state->IsMaximized());

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleMaximized,
                                        {});
    EXPECT_TRUE(window_state->IsMaximized());
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowMinimize, {});
    EXPECT_FALSE(window_state->IsMaximized());
    EXPECT_TRUE(window_state->IsMinimized());
    window_state->Restore();
    window_state->Activate();

    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleFullscreen,
                                        {});
    EXPECT_TRUE(window_state->IsFullscreen());
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                        {});
    EXPECT_TRUE(window_state->IsSnapped());
    EXPECT_FALSE(window_state->IsFullscreen());
    controller_->PerformActionIfEnabled(AcceleratorAction::kToggleFullscreen,
                                        {});
    controller_->PerformActionIfEnabled(
        AcceleratorAction::kWindowCycleSnapRight, {});
    EXPECT_TRUE(window_state->IsSnapped());
    EXPECT_FALSE(window_state->IsFullscreen());
  }
  {
    // Tests that window snap doesn't work while the window is minimized.
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowMinimize, {});
    EXPECT_TRUE(window_state->IsMinimized());
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                        {});
    EXPECT_TRUE(window_state->IsMinimized());

    // Unminimize the window. Now window snap should work.
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowMinimize, {});
    EXPECT_FALSE(window_state->IsMinimized());
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                        {});
    EXPECT_TRUE(window_state->IsSnapped());
  }
}

// Tests that if the screen is upside down, the left/right accelerators go to
// the physical left/top side of the screen.
TEST_F(AcceleratorControllerTest, WindowSnapUpsideDown) {
  const int64_t primary_display_id =
      WindowTreeHostManager::GetPrimaryDisplayId();

  // Make the display upside down.
  DisplayConfigurationController* controller =
      Shell::Get()->display_configuration_controller();
  controller->SetDisplayRotation(
      primary_display_id, display::Display::ROTATE_180,
      display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_SYNC);
  display::Display current_display =
      display_manager()->GetDisplayForId(primary_display_id);
  ASSERT_TRUE(current_display.is_landscape());
  ASSERT_FALSE(chromeos::IsDisplayLayoutPrimary(current_display));

  // Snap the window. Test that it goes to the physical left/right as expected.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  EXPECT_EQ(gfx::Point(0, 0), window->GetBoundsInScreen().origin());

  gfx::Rect work_area_bounds = current_display.work_area();
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  EXPECT_EQ(work_area_bounds.top_right(),
            window->GetBoundsInScreen().top_right());

  // Make the display 90 degrees (upside down vertically).
  controller->SetDisplayRotation(
      primary_display_id, display::Display::ROTATE_90,
      display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_SYNC);
  current_display = display_manager()->GetDisplayForId(primary_display_id);
  ASSERT_FALSE(current_display.is_landscape());
  ASSERT_FALSE(chromeos::IsDisplayLayoutPrimary(current_display));

  window = CreateAppWindow(gfx::Rect(300, 300));
  work_area_bounds = current_display.work_area();

  // Snap the window. Test that it goes to the physical top/bottom as expected.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  EXPECT_EQ(gfx::Point(0, 0), window->GetBoundsInScreen().origin());
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  EXPECT_EQ(work_area_bounds.bottom_left(),
            window->GetBoundsInScreen().bottom_left());
}

// Tests that window snapping works.
TEST_F(AcceleratorControllerTest, TestRepeatedSnap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();

  // Snap right.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  gfx::Rect normal_bounds = window_state->GetRestoreBoundsInParent();
  gfx::Rect expected_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kSecondary);
  EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsSnapped());
  // Snap right again ->> becomes normal.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(normal_bounds.ToString(), window->bounds().ToString());
  // Snap right.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  EXPECT_TRUE(window_state->IsSnapped());
  // Snap left.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  EXPECT_TRUE(window_state->IsSnapped());
  expected_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kPrimary);
  EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
  // Snap left again ->> becomes normal.
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(normal_bounds.ToString(), window->bounds().ToString());
}

class AcceleratorControllerTestWithClamshellSplitView
    : public AcceleratorControllerTest {
 public:
  AcceleratorControllerTestWithClamshellSplitView() = default;
  AcceleratorControllerTestWithClamshellSplitView(
      const AcceleratorControllerTestWithClamshellSplitView&) = delete;
  AcceleratorControllerTestWithClamshellSplitView& operator=(
      const AcceleratorControllerTestWithClamshellSplitView&) = delete;
  ~AcceleratorControllerTestWithClamshellSplitView() override = default;

 protected:
  // Note: These functions assume the default display resolution 800x600.
  void EnterOverviewAndDragToSnapLeft(aura::Window* window) {
    EnterOverviewAndDragTo(window, gfx::Point(0, 300));
  }
  void EnterOverviewAndDragToSnapRight(aura::Window* window) {
    EnterOverviewAndDragTo(window, gfx::Point(799, 300));
  }

 private:
  void EnterOverviewAndDragTo(aura::Window* window,
                              const gfx::Point& destination) {
    CHECK(!OverviewController::Get()->InOverviewSession());
    ToggleOverview();

    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(gfx::ToRoundedPoint(
        GetOverviewItemForWindow(window)->target_bounds().CenterPoint()));
    generator->DragMouseTo(destination);
  }
};

TEST_F(AcceleratorControllerTestWithClamshellSplitView, WindowSnapUma) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 20, 20)));
  // Some test cases use clamshell split view, for which we need a second window
  // so overview will be nonempty. Otherwise split view will end when it starts.
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  base::HistogramBase::Count left_clamshell_no_overview = 0;
  base::HistogramBase::Count left_clamshell_overview = 0;
  base::HistogramBase::Count left_tablet = 0;
  base::HistogramBase::Count right_clamshell_no_overview = 0;
  base::HistogramBase::Count right_clamshell_overview = 0;
  base::HistogramBase::Count right_tablet = 0;
  // Performs |action|, checks that |window1| is in |target_window1_state_type|,
  // and verifies metrics. Output of failed expectations includes |description|.
  const auto test = [&](const char* description, AcceleratorAction action,
                        WindowStateType target_window1_state_type) {
    SCOPED_TRACE(description);
    controller_->PerformActionIfEnabled(action, {});
    EXPECT_EQ(target_window1_state_type,
              WindowState::Get(window1.get())->GetStateType());
    EXPECT_EQ(
        left_clamshell_no_overview + left_clamshell_overview + left_tablet,
        user_action_tester.GetActionCount("Accel_Window_Snap_Left"));
    histogram_tester.ExpectTotalCount(
        "Ash.Accelerators.Actions.WindowCycleSnapLeft",
        left_clamshell_no_overview + left_clamshell_overview + left_tablet);
    EXPECT_EQ(
        right_clamshell_no_overview + right_clamshell_overview + right_tablet,
        user_action_tester.GetActionCount("Accel_Window_Snap_Right"));
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap,
        WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellNoOverview,
        left_clamshell_no_overview);
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap,
        WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellOverview,
        left_clamshell_overview);
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap, WindowSnapAcceleratorAction::kCycleLeftSnapInTablet,
        left_tablet);
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap,
        WindowSnapAcceleratorAction::kCycleRightSnapInClamshellNoOverview,
        right_clamshell_no_overview);
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap,
        WindowSnapAcceleratorAction::kCycleRightSnapInClamshellOverview,
        right_clamshell_overview);
    histogram_tester.ExpectBucketCount(
        kAccelWindowSnap, WindowSnapAcceleratorAction::kCycleRightSnapInTablet,
        right_tablet);
  };

  // Alt+[, clamshell, no overview
  wm::ActivateWindow(window1.get());
  left_clamshell_no_overview = 1;
  test("Snap left, clamshell, no overview",
       AcceleratorAction::kWindowCycleSnapLeft,
       WindowStateType::kPrimarySnapped);
  left_clamshell_no_overview = 2;
  test("Unsnap left, clamshell, no overview",
       AcceleratorAction::kWindowCycleSnapLeft, WindowStateType::kNormal);
  // Alt+[, clamshell, overview
  EnterOverviewAndDragToSnapRight(window1.get());
  left_clamshell_overview = 1;
  test("Snap left, clamshell, overview",
       AcceleratorAction::kWindowCycleSnapLeft,
       WindowStateType::kPrimarySnapped);
  left_clamshell_overview = 2;
  test("Unsnap left, clamshell, overview",
       AcceleratorAction::kWindowCycleSnapLeft, WindowStateType::kNormal);
  // Alt+], clamshell, no overview
  right_clamshell_no_overview = 1;
  test("Snap right, clamshell, no overview",
       AcceleratorAction::kWindowCycleSnapRight,
       WindowStateType::kSecondarySnapped);
  right_clamshell_no_overview = 2;
  test("Unsnap right, clamshell, no overview",
       AcceleratorAction::kWindowCycleSnapRight, WindowStateType::kNormal);
  // Alt+], clamshell, overview
  EnterOverviewAndDragToSnapLeft(window1.get());
  right_clamshell_overview = 1;
  test("Snap right, clamshell, overview",
       AcceleratorAction::kWindowCycleSnapRight,
       WindowStateType::kSecondarySnapped);
  right_clamshell_overview = 2;
  test("Unsnap right, clamshell, overview",
       AcceleratorAction::kWindowCycleSnapRight, WindowStateType::kNormal);
  // Alt+[, tablet, no overview
  ShellTestApi().SetTabletModeEnabledForTest(true);
  left_tablet = 1;
  test("Snap left, tablet, no overview",
       AcceleratorAction::kWindowCycleSnapLeft,
       WindowStateType::kPrimarySnapped);
  ToggleOverview();
  left_tablet = 2;
  test("Unsnap left, tablet, no overview",
       AcceleratorAction::kWindowCycleSnapLeft, WindowStateType::kMaximized);
  // Alt+[, tablet, overview
  EnterOverviewAndDragToSnapRight(window1.get());
  left_tablet = 3;
  test("Snap left, tablet, overview", AcceleratorAction::kWindowCycleSnapLeft,
       WindowStateType::kPrimarySnapped);
  left_tablet = 4;
  test("Unsnap left, tablet, overview", AcceleratorAction::kWindowCycleSnapLeft,
       WindowStateType::kMaximized);
  // Alt+], tablet, no overview
  right_tablet = 1;
  test("Snap right, tablet, no overview",
       AcceleratorAction::kWindowCycleSnapRight,
       WindowStateType::kSecondarySnapped);
  ToggleOverview();
  right_tablet = 2;
  test("Unsnap right, tablet, no overview",
       AcceleratorAction::kWindowCycleSnapRight, WindowStateType::kMaximized);
  // Alt+], tablet, overview
  EnterOverviewAndDragToSnapLeft(window1.get());
  right_tablet = 3;
  test("Snap right, tablet, overview", AcceleratorAction::kWindowCycleSnapRight,
       WindowStateType::kSecondarySnapped);
  right_tablet = 4;
  test("Unsnap right, tablet, overview",
       AcceleratorAction::kWindowCycleSnapRight, WindowStateType::kMaximized);
}

TEST_F(AcceleratorControllerTestWithClamshellSplitView,
       WindowSnapOrientationUma) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state = WindowState::Get(window.get());
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state2 = WindowState::Get(window2.get());
  base::HistogramTester histogram_tester;
  constexpr char kSnapWindowDeviceOrientationHistogram[] =
      "Ash.Window.Snap.DeviceOrientation";
  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 0);

  window_state->Activate();
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  gfx::Rect expected_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kPrimary);
  EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 0);

  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapRight,
                                      {});
  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 2);

  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 0);

  window_state2->Activate();
  UpdateDisplay("800x600/l");
  controller_->PerformActionIfEnabled(AcceleratorAction::kWindowCycleSnapLeft,
                                      {});
  histogram_tester.ExpectBucketCount(
      kSnapWindowDeviceOrientationHistogram,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 1);
}

TEST_F(AcceleratorControllerTest, RotateScreen) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display::Rotation initial_rotation =
      GetActiveDisplayRotation(display.id());
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();

  EXPECT_FALSE(accessibility_controller
                   ->HasDisplayRotationAcceleratorDialogBeenAccepted());
  PressAndReleaseKey(ui::VKEY_BROWSER_REFRESH,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  // Dialog should be open.
  EXPECT_TRUE(IsConfirmationDialogOpen());
  // Cancel on the dialog should have no effect.
  CancelConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(accessibility_controller
                   ->HasDisplayRotationAcceleratorDialogBeenAccepted());

  display::Display::Rotation rotation_after_cancel =
      GetActiveDisplayRotation(display.id());
  // Screen rotation should not have been triggered.
  EXPECT_EQ(initial_rotation, rotation_after_cancel);

  // Use short cut again.
  PressAndReleaseKey(ui::VKEY_BROWSER_REFRESH,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(IsConfirmationDialogOpen());
  AcceptConfirmationDialog();
  base::RunLoop().RunUntilIdle();

  // Dialog should be closed.
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(accessibility_controller
                  ->HasDisplayRotationAcceleratorDialogBeenAccepted());
  display::Display::Rotation rotation_after_accept =
      GetActiveDisplayRotation(display.id());
  // |new_rotation| is determined by the AcceleratorController.
  EXPECT_NE(initial_rotation, rotation_after_accept);
}

// Tests that using the keyboard shortcut to rotate the display while the device
// is in physical tablet state behaves like a request to lock the user
// orientation to the next rotation of the internal display, and disables auto-
// rotation.
TEST_F(AcceleratorControllerTest, RotateScreenInPhysicalTabletState) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ShellTestApi().SetTabletModeEnabledForTest(true);
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_TRUE(tablet_mode_controller->is_in_tablet_physical_state());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  TriggerRotateScreenShortcut();

  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            screen_orientation_controller->GetCurrentOrientation());

  // When the device is no longer used as a tablet, the original rotation will
  // be restored.
  ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(tablet_mode_controller->is_in_tablet_physical_state());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());
  // User rotation lock remains in place to be restored again when the device
  // goes to physical tablet state again.
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());
}

// Tests that using the keyboard shortcut to rotate the display while
// kSupportsClamshellAutoRotation is set in the device behaves like a request to
// lock the user orientation to the next rotation of the internal display, and
// disables auto-rotation.
TEST_F(AcceleratorControllerTest,
       RotateScreenWithClamshellAutoRotationSupported) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSupportsClamshellAutoRotation);

  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_TRUE(screen_orientation_controller->IsAutoRotationAllowed());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  TriggerRotateScreenShortcut();

  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            screen_orientation_controller->GetCurrentOrientation());
}

// Tests the behavior of the shortcut when the active window requests to lock
// the rotation to a particular orientation.
TEST_F(AcceleratorControllerTest, RotateScreenWithWindowLockingOrientation) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ShellTestApi().SetTabletModeEnabledForTest(true);
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_TRUE(tablet_mode_controller->is_in_tablet_physical_state());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  auto win0 = CreateAppWindow(gfx::Rect{100, 300});
  auto win1 = CreateAppWindow(gfx::Rect{200, 200});
  screen_orientation_controller->LockOrientationForWindow(
      win0.get(), chromeos::OrientationType::kPortraitPrimary);
  screen_orientation_controller->LockOrientationForWindow(
      win1.get(), chromeos::OrientationType::kLandscape);

  // `win0` requests to lock the orientation to only portrait-primary. The
  // shortcut therefore won't be able to change the current rotation at all.
  wm::ActivateWindow(win0.get());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            screen_orientation_controller->GetCurrentOrientation());

  TriggerRotateScreenShortcut();
  // Nothing happens; user rotation is still not locked, but the rotation is
  // app-locked.
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            screen_orientation_controller->GetCurrentOrientation());

  // Activate `win1` which allows any landscape orientations (either primary or
  // secondary). The shortcut will switch between the two allowed orientations
  // only.
  wm::ActivateWindow(win1.get());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  TriggerRotateScreenShortcut();
  // User rotation will now be locked.
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            screen_orientation_controller->GetCurrentOrientation());
  TriggerRotateScreenShortcut();
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  // Hook a mouse device, exiting tablet mode to clamshell mode (but remaining
  // in a tablet physical state). Expect that the shortcut changes the user
  // rotation lock in all directions regardless of which window is active, even
  // those that requested window rotation locks.
  TabletModeControllerTestApi().AttachExternalMouse();
  EXPECT_TRUE(tablet_mode_controller->is_in_tablet_physical_state());
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  wm::ActivateWindow(win0.get());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());
  TriggerRotateScreenShortcut();
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            screen_orientation_controller->GetCurrentOrientation());
  TriggerRotateScreenShortcut();
  EXPECT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            screen_orientation_controller->GetCurrentOrientation());

  wm::ActivateWindow(win1.get());
  TriggerRotateScreenShortcut();
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            screen_orientation_controller->GetCurrentOrientation());
  TriggerRotateScreenShortcut();
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());
}

TEST_F(AcceleratorControllerTest, AutoRepeat) {
  ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  ui::TestAcceleratorTarget target_a;
  controller_->Register({accelerator_a}, &target_a);
  ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  ui::TestAcceleratorTarget target_b;
  controller_->Register({accelerator_b}, &target_b);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, target_a.accelerator_count());
  EXPECT_EQ(0, target_a.accelerator_repeat_count());

  // Long press should generate one
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_EQ(2, target_a.accelerator_non_repeat_count());
  EXPECT_EQ(1, target_a.accelerator_repeat_count());
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_EQ(2, target_a.accelerator_non_repeat_count());
  EXPECT_EQ(2, target_a.accelerator_repeat_count());
  generator->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(2, target_a.accelerator_non_repeat_count());
  EXPECT_EQ(2, target_a.accelerator_repeat_count());

  // Long press was intercepted by another key press.
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  generator->PressKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator->ReleaseKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, target_b.accelerator_non_repeat_count());
  EXPECT_EQ(0, target_b.accelerator_repeat_count());
  EXPECT_EQ(4, target_a.accelerator_non_repeat_count());
  EXPECT_EQ(4, target_a.accelerator_repeat_count());
}

TEST_F(AcceleratorControllerTest, Previous) {
  PressAndReleaseKey(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);

  EXPECT_EQ(ui::VKEY_VOLUME_MUTE, GetPreviousAccelerator().key_code());
  EXPECT_EQ(ui::EF_NONE, GetPreviousAccelerator().modifiers());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(ui::VKEY_TAB, GetPreviousAccelerator().key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, GetPreviousAccelerator().modifiers());
}

TEST_F(AcceleratorControllerTest, DontRepeatToggleFullscreen) {
  const AcceleratorData accelerators[] = {
      {true, ui::VKEY_J, ui::EF_ALT_DOWN, AcceleratorAction::kToggleFullscreen},
      {true, ui::VKEY_K, ui::EF_ALT_DOWN, AcceleratorAction::kToggleFullscreen},
  };
  test_api_->RegisterAccelerators(accelerators);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  views::Widget* widget = new views::Widget;
  params.context = GetContext();
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();
  widget->GetNativeView()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize |
          aura::client::kResizeBehaviorCanFullscreen);

  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowState* window_state = WindowState::Get(widget->GetNativeView());

  // Toggling not suppressed.
  generator->PressKey(ui::VKEY_J, ui::EF_ALT_DOWN);
  EXPECT_TRUE(window_state->IsFullscreen());

  // The same accelerator - toggling suppressed.
  generator->PressKey(ui::VKEY_J, ui::EF_ALT_DOWN | ui::EF_IS_REPEAT);
  EXPECT_TRUE(window_state->IsFullscreen());

  // Different accelerator.
  generator->PressKey(ui::VKEY_K, ui::EF_ALT_DOWN);
  EXPECT_FALSE(window_state->IsFullscreen());
}

TEST_F(AcceleratorControllerTest, DontToggleFullscreenWhenOverviewStarts) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(400, 400)));

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Toggle overview and fullscreen immediately after.
  generator->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  generator->PressKey(ui::VKEY_ZOOM, ui::EF_NONE);
  EXPECT_FALSE(WindowState::Get(widget->GetNativeWindow())->IsFullscreen());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(OverviewController::Get()->overview_session()->IsWindowInOverview(
      widget->GetNativeWindow()));
}

// Tests that window shortcuts don't work on a minimized, i.e. not visible,
// window in overview.
TEST_F(AcceleratorControllerTest, MinimizedWindowInOverview) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  ToggleOverview();
  GetEventGenerator()->PressKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(window_state->IsMinimized());
}

// TODO(oshima): Fix this test to use EventGenerator.
TEST_F(AcceleratorControllerTest, ProcessOnce) {
  // The IME event filter interferes with the basic key event propagation we
  // attempt to do here, so we disable it.
  DisableIME();
  ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  ui::TestAcceleratorTarget target;
  controller_->Register({accelerator_a}, &target);

  // The accelerator is processed only once.
  ui::EventSink* sink =
      Shell::GetPrimaryRootWindow()->GetHost()->GetEventSink();

  ui::KeyEvent key_event1(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  ui::EventDispatchDetails details = sink->OnEventFromSource(&key_event1);
  EXPECT_TRUE(key_event1.handled() || details.dispatcher_destroyed);

  ui::KeyEvent key_event2 = ui::KeyEvent::FromCharacter(
      'A', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
  details = sink->OnEventFromSource(&key_event2);
  EXPECT_FALSE(key_event2.handled() || details.dispatcher_destroyed);

  ui::KeyEvent key_event3(ui::EventType::kKeyReleased, ui::VKEY_A, ui::EF_NONE);
  details = sink->OnEventFromSource(&key_event3);
  EXPECT_FALSE(key_event3.handled() || details.dispatcher_destroyed);
  EXPECT_EQ(1, target.accelerator_count());
}

TEST_F(AcceleratorControllerTest, GlobalAccelerators) {
  // CycleBackward
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  // CycleForward
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  // CycleLinear
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE)));

  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    base::UserActionTester user_action_tester;
    auto* history = controller_->GetAcceleratorHistory();

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_TRUE(ProcessInController(volume_mute));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_EQ(volume_mute, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_TRUE(ProcessInController(volume_down));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_EQ(volume_down, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
    EXPECT_TRUE(ProcessInController(volume_up));
    EXPECT_EQ(volume_up, history->current_accelerator());
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  }
  // Brightness
  // ui::VKEY_BRIGHTNESS_DOWN/UP are not defined on Windows.
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate;
    SetBrightnessControlDelegate(
        std::unique_ptr<BrightnessControlDelegate>(delegate));
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessInController(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessInController(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }

  // Keyboard brightness
  const ui::Accelerator alt_brightness_down(ui::VKEY_BRIGHTNESS_DOWN,
                                            ui::EF_ALT_DOWN);
  const ui::Accelerator alt_brightness_up(ui::VKEY_BRIGHTNESS_UP,
                                          ui::EF_ALT_DOWN);
  const ui::Accelerator toggle_keyboard_backlight(ui::VKEY_KBD_BACKLIGHT_TOGGLE,
                                                  ui::EF_NONE);
  {
    EXPECT_TRUE(ProcessInController(alt_brightness_down));
    EXPECT_TRUE(ProcessInController(alt_brightness_up));
    EXPECT_TRUE(ProcessInController(toggle_keyboard_backlight));

    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate;
    SetKeyboardBrightnessControlDelegate(
        std::unique_ptr<KeyboardBrightnessControlDelegate>(delegate));

    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_TRUE(ProcessInController(alt_brightness_down));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_brightness_down, delegate->last_accelerator());

    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_TRUE(ProcessInController(alt_brightness_up));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_brightness_up, delegate->last_accelerator());

    EXPECT_EQ(0, delegate->handle_toggle_keyboard_backlight_count());
    EXPECT_TRUE(ProcessInController(toggle_keyboard_backlight));
    EXPECT_EQ(1, delegate->handle_toggle_keyboard_backlight_count());
    EXPECT_EQ(toggle_keyboard_backlight, delegate->last_accelerator());
  }

  // Exit
  ExitWarningHandler* ewh = test_api_->GetExitWarningHandler();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);

  // New tab
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));

  // New incognito window
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New window
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN)));

  // Restore tab
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // Show task manager
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN)));

  // Open file manager
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

  // Lock screen
  // NOTE: Accelerators that do not work on the lock screen need to be
  // tested before the sequence below is invoked because it causes a side
  // effect of locking the screen.
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_L, ui::EF_COMMAND_DOWN)));

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
}

TEST_F(AcceleratorControllerTest, GlobalAcceleratorsToggleAppList) {
  test_api_->SetCanHandleLauncher(true);

  // The press event should not toggle the AppList, the release should instead.
  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ui::VKEY_LWIN, GetCurrentAccelerator().key_code());
  GetAppListTestHelper()->CheckVisibility(false);

  EXPECT_TRUE(ProcessInController(
      CreateReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_TRUE(ProcessInController(
      CreateReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // The press of VKEY_BROWSER_SEARCH should toggle the AppList
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_BROWSER_SEARCH, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(ProcessInController(
      CreateReleaseAccelerator(ui::VKEY_BROWSER_SEARCH, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Verifies VKEY_RWIN triggers AppList, too. This happens if modifier
  // keys are swapped.
  GetAppListTestHelper()->DismissAndRunLoop();
  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_RWIN, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ui::VKEY_RWIN, GetCurrentAccelerator().key_code());
  GetAppListTestHelper()->CheckVisibility(false);

  EXPECT_TRUE(ProcessInController(
      CreateReleaseAccelerator(ui::VKEY_RWIN, ui::EF_NONE)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(ui::VKEY_RWIN, GetPreviousAccelerator().key_code());
}

TEST_F(AcceleratorControllerTest, GlobalAcceleratorsToggleQuickSettings) {
  UnifiedSystemTray* tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();

  auto* generator = GetEventGenerator();

  // Pressing accelerator once should show the quick settings bubble.
  generator->PressKey(ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tray->IsBubbleShown());

  // Pressing accelerator a second time should dismiss the bubble.
  generator->PressKey(ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tray->IsBubbleShown());
}

TEST_F(AcceleratorControllerTest, ToggleMultitaskMenu) {
  // Accelerators behind a flag should also be accompanied by the
  // `kShortcutCustomization` to support dynamic accelerator registration.
  base::test::ScopedFeatureList scoped_feature_list(
      ::features::kShortcutCustomization);

  // Simulate fake user login to ensure pref registration is done correctly.
  SimulateUserLogin("fakeuser");
  // Enabling `kShortcutCustomization` will start letting
  // `AcceleratorControllerImpl` to observe changes to the accelerator list.
  // This includes accelerators added by enabling flags.
  test_api_->ObserveAcceleratorUpdates();

  // Typically updating flags will restart chrome and re-initialize accelerator
  // targeting.
  Shell::Get()->ash_accelerator_configuration()->Initialize();

  std::unique_ptr<aura::Window> window = CreateAppWindow();
  ui::Accelerator accelerator(ui::VKEY_Z, ui::EF_COMMAND_DOWN);
  // Pressing accelerator once should show the multitask menu.
  EXPECT_TRUE(ProcessInController(accelerator));
  auto* frame_view = NonClientFrameViewAsh::Get(window.get());
  auto* size_button = static_cast<chromeos::FrameSizeButton*>(
      frame_view->GetHeaderView()->caption_button_container()->size_button());
  ASSERT_TRUE(size_button->IsMultitaskMenuShown());

  // Pressing accelerator a second time should close the menu.
  EXPECT_TRUE(ProcessInController(accelerator));
  ASSERT_FALSE(size_button->IsMultitaskMenuShown());
}

class GlobalAcceleratorsToggleLauncher
    : public AcceleratorControllerTest,
      public testing::WithParamInterface<
          std::pair<ui::KeyboardCode, ui::Accelerator::KeyState>> {
 public:
  GlobalAcceleratorsToggleLauncher() {
    std::tie(key_, key_state_) = GetParam();
  }

 protected:
  ui::KeyboardCode key_;
  ui::Accelerator::KeyState key_state_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    GlobalAcceleratorsToggleLauncher,
    testing::Values(std::make_pair(ui::VKEY_LWIN,
                                   ui::Accelerator::KeyState::RELEASED),
                    std::make_pair(ui::VKEY_BROWSER_SEARCH,
                                   ui::Accelerator::KeyState::PRESSED),
                    std::make_pair(ui::VKEY_ALL_APPLICATIONS,
                                   ui::Accelerator::KeyState::PRESSED)));

TEST_P(GlobalAcceleratorsToggleLauncher, ToggleLauncher) {
  test_api_->SetCanHandleLauncher(true);

  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(key_, ui::EF_NONE, key_state_)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(key_, ui::EF_NONE, key_state_)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(GlobalAcceleratorsToggleLauncher, PreventProcessingShortcuts) {
  test_api_->SetCanHandleLauncher(true);

  // Set Controller to block all shortcuts and try to toggle the productivity
  // launcher
  controller_->SetPreventProcessingAccelerators(true);

  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(key_, ui::EF_NONE, key_state_)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Allow all shortcuts and redo the test
  controller_->SetPreventProcessingAccelerators(false);

  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(key_, ui::EF_NONE, key_state_)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(key_, ui::EF_NONE, key_state_)));
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AcceleratorControllerTest, ImeGlobalAccelerators) {
  ASSERT_EQ(0u, Shell::Get()->ime_controller()->GetVisibleImes().size());

  // Cycling IME is blocked because there is nothing to switch to.
  ui::Accelerator control_space_down(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  ui::Accelerator control_space_up(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  control_space_up.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::Accelerator control_shift_space(ui::VKEY_SPACE,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(ProcessInController(control_space_down));
  EXPECT_FALSE(ProcessInController(control_space_up));
  EXPECT_FALSE(ProcessInController(control_shift_space));

  // Adding only a not visible IME doesn't make IME accelerators available.
  AddNotVisibleTestIme();
  ASSERT_EQ(0u, Shell::Get()->ime_controller()->GetVisibleImes().size());
  EXPECT_FALSE(ProcessInController(control_space_down));
  EXPECT_FALSE(ProcessInController(control_space_up));
  EXPECT_FALSE(ProcessInController(control_shift_space));

  // Cycling IME works when there are IMEs available.
  AddTestImes();
  EXPECT_TRUE(ProcessInController(control_space_down));
  EXPECT_TRUE(ProcessInController(control_space_up));
  EXPECT_TRUE(ProcessInController(control_shift_space));

  // Adding the not visible IME back doesn't block cycling.
  AddNotVisibleTestIme();
  EXPECT_TRUE(ProcessInController(control_space_down));
  EXPECT_TRUE(ProcessInController(control_space_up));
  EXPECT_TRUE(ProcessInController(control_shift_space));
}

// TODO(nona|mazda): Remove this when crbug.com/139556 in a better way.
TEST_F(AcceleratorControllerTest, ImeGlobalAcceleratorsWorkaround139556) {
  // The workaround for crbug.com/139556 depends on the fact that we don't
  // use Shift+Alt+Enter/Space with EventType::kKeyPressed as an accelerator.
  // Test it.
  const ui::Accelerator shift_alt_return_press(
      ui::VKEY_RETURN, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(shift_alt_return_press));
  const ui::Accelerator shift_alt_space_press(
      ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(shift_alt_space_press));
}

TEST_F(AcceleratorControllerTest, PreferredReservedAccelerators) {
  // Power key is reserved on chromeos.
  EXPECT_TRUE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_POWER, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_POWER, ui::EF_NONE)));

  // Lock key is reserved on chromeos.
  EXPECT_TRUE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_SLEEP, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_SLEEP, ui::EF_NONE)));
  EXPECT_TRUE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_F13, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_F13, ui::EF_NONE)));

  // ALT+Tab are not reserved but preferred.
  EXPECT_FALSE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_FALSE(controller_->IsReserved(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  EXPECT_TRUE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_TRUE(controller_->IsPreferred(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

  // Others are not reserved nor preferred
  EXPECT_FALSE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_SNAPSHOT, ui::EF_NONE)));
  EXPECT_FALSE(controller_->IsPreferred(
      ui::Accelerator(ui::VKEY_SNAPSHOT, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsReserved(ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
  EXPECT_FALSE(
      controller_->IsPreferred(ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
}

TEST_F(AcceleratorControllerTest, SideVolumeButtonLocation) {
  // |side_volume_button_location_| should be empty when location info file
  // doesn't exist.
  EXPECT_TRUE(test_api_->GetSideVolumeButtonLocation().region.empty());
  EXPECT_TRUE(test_api_->GetSideVolumeButtonLocation().side.empty());

  // Tests that |side_volume_button_location_| is read correctly if the location
  // file exists.
  base::Value::Dict location;
  location.Set(kVolumeButtonRegion, kVolumeButtonRegionScreen);
  location.Set(kVolumeButtonSide, kVolumeButtonSideLeft);
  std::string json_location;
  base::JSONWriter::Write(location, &json_location);
  base::ScopedTempDir file_tmp_dir;
  ASSERT_TRUE(file_tmp_dir.CreateUniqueTempDir());
  base::FilePath file_path = file_tmp_dir.GetPath().Append("location.json");
  ASSERT_TRUE(WriteJsonFile(file_path, json_location));
  EXPECT_TRUE(base::PathExists(file_path));
  test_api_->SetSideVolumeButtonFilePath(file_path);
  EXPECT_EQ(kVolumeButtonRegionScreen,
            test_api_->GetSideVolumeButtonLocation().region);
  EXPECT_EQ(kVolumeButtonSideLeft,
            test_api_->GetSideVolumeButtonLocation().side);
  base::DeleteFile(file_path);
}

// Tests the histogram of volume adjustment in tablet mode.
TEST_F(AcceleratorControllerTest, TabletModeVolumeAdjustHistogram) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::HistogramTester histogram_tester;
  const ui::Accelerator kVolumeDown(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator kVolumeUp(ui::VKEY_VOLUME_UP, ui::EF_NONE);

  // Starts with volume up but ends with an overall-decreased volume.
  ProcessInController(kVolumeUp);
  ProcessInController(kVolumeDown);
  ProcessInController(kVolumeDown);
  EXPECT_TRUE(test_api_->TriggerTabletModeVolumeAdjustTimer());
  histogram_tester.ExpectBucketCount(
      kTabletCountOfVolumeAdjustType,
      TabletModeVolumeAdjustType::kAccidentalAdjustWithSwapEnabled, 1);

  // Starts with volume up and ends with an overall-increased volume.
  ProcessInController(kVolumeUp);
  ProcessInController(kVolumeUp);
  ProcessInController(kVolumeUp);
  EXPECT_TRUE(test_api_->TriggerTabletModeVolumeAdjustTimer());
  histogram_tester.ExpectBucketCount(
      kTabletCountOfVolumeAdjustType,
      TabletModeVolumeAdjustType::kNormalAdjustWithSwapEnabled, 1);
}

class SideVolumeButtonAcceleratorTest
    : public AcceleratorControllerTest,
      public testing::WithParamInterface<std::pair<std::string, std::string>> {
 public:
  // Input device id of the side volume button.
  static constexpr int kSideVolumeButtonId = 7;

  SideVolumeButtonAcceleratorTest()
      : region_(GetParam().first), side_(GetParam().second) {}

  SideVolumeButtonAcceleratorTest(const SideVolumeButtonAcceleratorTest&) =
      delete;
  SideVolumeButtonAcceleratorTest& operator=(
      const SideVolumeButtonAcceleratorTest&) = delete;

  ~SideVolumeButtonAcceleratorTest() override = default;

  void SetUp() override {
    AcceleratorControllerTest::SetUp();
    ash::TabletModeControllerTestApi().EnterTabletMode();
    test_api_->SetSideVolumeButtonLocation(region_, side_);
    ui::DeviceDataManagerTestApi().SetUncategorizedDevices({ui::InputDevice(
        kSideVolumeButtonId, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
        "cros_ec_buttons")});
  }

  bool IsLeftOrRightSide() const {
    return side_ == kVolumeButtonSideLeft || side_ == kVolumeButtonSideRight;
  }

  bool IsOnKeyboard() const { return region_ == kVolumeButtonRegionKeyboard; }

 private:
  std::string region_, side_;
};

// Tests the the action of side volume button will get flipped in corresponding
// screen orientation.
TEST_P(SideVolumeButtonAcceleratorTest, FlipSideVolumeButtonAction) {
  display::test::ScopedSetInternalDisplayId set_internal(
      display_manager(), GetPrimaryDisplay().id());

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  base::UserActionTester user_action_tester;
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  ASSERT_EQ(ui::ED_UNKNOWN_DEVICE, volume_down.source_device_id());
  ProcessInController(volume_down);
  // Tests that the VOLUME_DOWN accelerator always goes to decrease the volume
  // if it is not from the side volume button.
  EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  user_action_tester.ResetCounts();

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_VOLUME_DOWN,
                     ui::DomCode::VOLUME_DOWN, /*flags=*/0, /*dom_key=*/2099727,
                     base::TimeTicks::Now());
  event.set_source_device_id(kSideVolumeButtonId);
  const ui::Accelerator volume_down_from_side_volume_button(event);
  ProcessInController(volume_down_from_side_volume_button);
  // Tests that the action of side volume button will get flipped in landscape
  // primary if the the button is at the left or right of keyboard.
  if (IsOnKeyboard() && IsLeftOrRightSide())
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  else
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  user_action_tester.ResetCounts();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  ProcessInController(volume_down_from_side_volume_button);
  // Tests that the action of side volume button will not be flipped in portrait
  // primary if the button is at the left or right of screen. Otherwise, the
  // action will be flipped.
  if (!IsOnKeyboard() && IsLeftOrRightSide())
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  else
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  user_action_tester.ResetCounts();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  ProcessInController(volume_down_from_side_volume_button);
  // Tests that the action of side volume button will not be flipped in
  // landscape secondary if the button is at the left or right of keyboard.
  // Otherwise, the action will be flipped.
  if (IsOnKeyboard() && IsLeftOrRightSide())
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  else
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  user_action_tester.ResetCounts();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  ProcessInController(volume_down_from_side_volume_button);
  // Tests that the action of side volume button will be flipped in portrait
  // secondary if the buttonis at the left or right of screen.
  if (!IsOnKeyboard() && IsLeftOrRightSide())
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  else
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
}

INSTANTIATE_TEST_SUITE_P(
    AshSideVolumeButton,
    SideVolumeButtonAcceleratorTest,
    testing::ValuesIn(
        {std::pair<std::string, std::string>(kVolumeButtonRegionKeyboard,
                                             kVolumeButtonSideLeft),
         std::pair<std::string, std::string>(kVolumeButtonRegionKeyboard,
                                             kVolumeButtonSideRight),
         std::pair<std::string, std::string>(kVolumeButtonRegionKeyboard,
                                             kVolumeButtonSideBottom),
         std::pair<std::string, std::string>(kVolumeButtonRegionScreen,
                                             kVolumeButtonSideLeft),
         std::pair<std::string, std::string>(kVolumeButtonRegionScreen,
                                             kVolumeButtonSideRight),
         std::pair<std::string, std::string>(kVolumeButtonRegionScreen,
                                             kVolumeButtonSideTop),
         std::pair<std::string, std::string>(kVolumeButtonRegionScreen,
                                             kVolumeButtonSideBottom)}));

TEST_F(AcceleratorControllerTest, PressAndReleasePowerButtonWithFunctionKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kModifierSplit, features::kPeripheralCustomization,
       features::kInputDeviceSettingsSplit},
      {});
  auto reset = switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  const int kKeyboardDeviceIdWithFunction = 123;
  const int kKeyboardDeviceId = 456;
  const ui::KeyboardDevice keyboard_with_function(
      kKeyboardDeviceIdWithFunction, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"test keyboard with function key",
      /*phys=*/"",
      base::FilePath("/devices/platform/i8042/serio2/input/input1"),
      /*vendor=*/-1,
      /*product=*/-1, /*version=*/-1,
      /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  const ui::KeyboardDevice keyboard(
      kKeyboardDeviceId, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"test keyboard with function key",
      /*phys=*/"",
      base::FilePath("/devices/platform/i8042/serio2/input/input1"),
      /*vendor=*/-1,
      /*product=*/-1, /*version=*/-1,
      /*has_assistant_key=*/true,
      /*has_function_key=*/false);

  // Reset the state of the device manager.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard_with_function});

  // Create an event with a keyboard that has function key. The controller
  // should not process the lock key event.
  ui::KeyEvent press_event_with_function(ui::EventType::kKeyPressed,
                                         ui::VKEY_F13,
                                         /*flags=*/ui::EF_NONE);
  press_event_with_function.set_source_device_id(kKeyboardDeviceIdWithFunction);
  const ui::Accelerator press_f13_with_function(press_event_with_function);
  EXPECT_FALSE(ProcessInController(press_f13_with_function));

  // Test releasing F13 with fn key.
  ui::KeyEvent release_event_with_function(ui::EventType::kKeyReleased,
                                           ui::VKEY_F13,
                                           /*flags=*/ui::EF_NONE);
  release_event_with_function.set_source_device_id(
      kKeyboardDeviceIdWithFunction);
  const ui::Accelerator release_f13_with_function(release_event_with_function);

  EXPECT_FALSE(ProcessInController(release_f13_with_function));

  // Reset the state of the device manager.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {keyboard_with_function, keyboard});
  // Create an event with a keyboard that does not have function key. The
  // controller should process the lock key event.
  ui::KeyEvent press_event(ui::EventType::kKeyPressed, ui::VKEY_F13,
                           /*flags=*/ui::EF_NONE);
  press_event.set_source_device_id(kKeyboardDeviceId);
  const ui::Accelerator press_f13(press_event);
  EXPECT_TRUE(ProcessInController(press_f13));

  // Test releaseing F13 without fn key.
  ui::KeyEvent release_event(ui::EventType::kKeyReleased, ui::VKEY_F13,
                             /*flags=*/ui::EF_NONE);
  release_event.set_source_device_id(kKeyboardDeviceId);
  const ui::Accelerator release_f13(press_event);
  EXPECT_TRUE(ProcessInController(release_f13));
}

TEST_F(AcceleratorControllerTest, ToggleCapsLockAcceleratorsWithFunctionKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kModifierSplit, features::kShortcutStateMachines,
       features::kPeripheralCustomization, features::kInputDeviceSettingsSplit},
      {});
  auto reset = switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  ASSERT_TRUE(nudge_manager);

  const int kKeyboardDeviceId = 123;
  const ui::KeyboardDevice keyboard(
      kKeyboardDeviceId, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"test keyboard with function key",
      /*phys=*/"",
      base::FilePath("/devices/platform/i8042/serio2/input/input1"),
      /*vendor=*/-1,
      /*product=*/-1, /*version=*/-1,
      /*has_assistant_key=*/true,
      /*has_function_key=*/true);

  // // Reset the state of the device manager.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});
  test_api_->SetCanHandleCapsLock(true);

  ImeControllerImpl* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  controller->SetClient(&client);
  EXPECT_EQ(0, client.set_caps_lock_count_);

  // Create an event with a keyboard that has function key. The controller
  // shouldn't process the capsLock key event.
  ui::KeyEvent press_event(ui::EventType::kKeyPressed, ui::VKEY_LWIN,
                           /*flags=*/ui::EF_ALT_DOWN);
  press_event.set_source_device_id(kKeyboardDeviceId);
  const ui::Accelerator press_search_after_alt(press_event);
  EXPECT_FALSE(ProcessInController(press_search_after_alt));

  ui::KeyEvent release_event(ui::EventType::kKeyReleased, ui::VKEY_LWIN,
                             /*flags=*/ui::EF_ALT_DOWN);
  release_event.set_source_device_id(kKeyboardDeviceId);
  const ui::Accelerator release_search_after_alt(release_event);
  EXPECT_FALSE(ProcessInController(release_search_after_alt));

  EXPECT_EQ(0, client.set_caps_lock_count_);
  EXPECT_FALSE(controller->IsCapsLockEnabled());

  // Notification showing caps lock blocked by function key should show up.
  EXPECT_TRUE(nudge_manager->GetNudgeIfShown(kCapsLockNoMatchNudgeId));
  nudge_manager->Cancel(kCapsLockNoMatchNudgeId);
  feature_list.Reset();
}

// Tests the AcceleratorAction::kToggleCapsLock accelerator.
TEST_F(AcceleratorControllerTest, ToggleCapsLockAccelerators) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kShortcutStateMachines);

  ImeControllerImpl* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  controller->SetClient(&client);
  EXPECT_EQ(0, client.set_caps_lock_count_);

  // 1. Press Alt, Press Search, Release Search, Release Alt.
  // Note when you press Alt then press search, the key_code at this point is
  // VKEY_LWIN (for search) and Alt is the modifier.
  const ui::Accelerator press_alt_then_search(ui::VKEY_LWIN, ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(press_alt_then_search));
  // When you release Search before Alt, the key_code is still VKEY_LWIN and
  // Alt is still the modifier.
  const ui::Accelerator release_search_before_alt(
      CreateReleaseAccelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN));
  EXPECT_TRUE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(1, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 2. Press Search, Press Alt, Release Search, Release Alt.
  const ui::Accelerator press_search_then_alt(ui::VKEY_MENU,
                                              ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_TRUE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(2, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 3. Press Alt, Press Search, Release Alt, Release Search.
  EXPECT_FALSE(ProcessInController(press_alt_then_search));
  const ui::Accelerator release_alt_before_search(
      CreateReleaseAccelerator(ui::VKEY_MENU, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(ProcessInController(release_alt_before_search));
  EXPECT_EQ(3, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 4. Press Search, Press Alt, Release Alt, Release Search.
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_TRUE(ProcessInController(release_alt_before_search));
  EXPECT_EQ(4, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 5. Press M, Press Alt, Press Search, Release Alt. After that CapsLock
  // should not be triggered. https://crbug.com/789283
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_M, ui::EF_NONE);
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  generator->PressKey(ui::VKEY_LWIN, ui::EF_ALT_DOWN);
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);
  generator->ReleaseKey(ui::VKEY_M, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_LWIN, ui::EF_ALT_DOWN);

  // 6. Toggle CapsLock shortcut should still work after the partial screenshot
  // shortcut is used. (https://crbug.com/920030)
  {
    // Press Ctrl+Shift+F5 then release to enter the partial screenshot session.
    const ui::Accelerator press_partial_screenshot_shortcut(
        ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
    EXPECT_TRUE(ProcessInController(press_partial_screenshot_shortcut));
    const ui::Accelerator release_partial_screenshot_shortcut =
        CreateReleaseAccelerator(ui::VKEY_MEDIA_LAUNCH_APP1,
                                 ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
    EXPECT_FALSE(ProcessInController(release_partial_screenshot_shortcut));

    // Press mouse left button, move mouse and release mouse button. Then
    // the partial screenshot is taken.
    generator->MoveMouseTo(0, 0);
    generator->PressLeftButton();
    generator->MoveMouseTo(10, 10);
    generator->ReleaseLeftButton();
    auto* capture_mode_controller = CaptureModeController::Get();
    EXPECT_TRUE(capture_mode_controller->IsActive());
    EXPECT_EQ(CaptureModeSource::kRegion, capture_mode_controller->source());

    // Press Search, Press Alt, Release Search, Release Alt. CapsLock should be
    // triggered.
    EXPECT_FALSE(ProcessInController(press_search_then_alt));
    EXPECT_TRUE(ProcessInController(release_search_before_alt));
    EXPECT_EQ(5, client.set_caps_lock_count_);
    EXPECT_TRUE(controller->IsCapsLockEnabled());
    controller->UpdateCapsLockState(false);
  }

  // 7. Toggle CapsLock shortcut should still work after fake events generated.
  // (https://crbug.com/918317).
  generator->PressKey(ui::VKEY_PROCESSKEY, ui::EF_IME_FABRICATED_KEY);
  generator->ReleaseKey(ui::VKEY_UNKNOWN, ui::EF_IME_FABRICATED_KEY);

  // Press Search, Press Alt, Release Search, Release Alt. CapsLock should be
  // triggered.
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_TRUE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(6, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);
}

// Tests the AcceleratorAction::kToggleCapsLock accelerator.
TEST_F(AcceleratorControllerTest, ToggleCapsLockAcceleratorsStateMachines) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kShortcutStateMachines);

  ImeControllerImpl* controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  controller->SetClient(&client);
  EXPECT_EQ(0, client.set_caps_lock_count_);

  // Following tests are testing valid uses of the capslock accelerator.
  test_api_->SetCanHandleCapsLock(true);

  // 1. Press Alt, Press Search, Release Search, Release Alt.
  // Note when you press Alt then press search, the key_code at this point is
  // VKEY_LWIN (for search) and Alt is the modifier.
  const ui::Accelerator press_alt_then_search(ui::VKEY_LWIN, ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(press_alt_then_search));
  // When you release Search before Alt, the key_code is still VKEY_LWIN and
  // Alt is still the modifier.
  const ui::Accelerator release_search_before_alt(
      CreateReleaseAccelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN));
  EXPECT_TRUE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(1, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 2. Press Search, Press Alt, Release Search, Release Alt.
  const ui::Accelerator press_search_then_alt(ui::VKEY_MENU,
                                              ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_TRUE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(2, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 3. Press Alt, Press Search, Release Alt, Release Search.
  EXPECT_FALSE(ProcessInController(press_alt_then_search));
  const ui::Accelerator release_alt_before_search(
      CreateReleaseAccelerator(ui::VKEY_MENU, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(ProcessInController(release_alt_before_search));
  EXPECT_EQ(3, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 4. Press Search, Press Alt, Release Alt, Release Search.
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_TRUE(ProcessInController(release_alt_before_search));
  EXPECT_EQ(4, client.set_caps_lock_count_);
  EXPECT_TRUE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // Following tests are testing invalid uses of the capslock accelerator.
  test_api_->SetCanHandleCapsLock(false);

  // 5. Press Search, Press Alt, Release Alt, Release search
  EXPECT_FALSE(ProcessInController(press_search_then_alt));
  EXPECT_FALSE(ProcessInController(release_alt_before_search));
  EXPECT_EQ(4, client.set_caps_lock_count_);
  EXPECT_FALSE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);

  // 6. Press Alt, Press Search, Release Search, Release Alt
  EXPECT_FALSE(ProcessInController(press_alt_then_search));
  EXPECT_FALSE(ProcessInController(release_search_before_alt));
  EXPECT_EQ(4, client.set_caps_lock_count_);
  EXPECT_FALSE(controller->IsCapsLockEnabled());
  controller->UpdateCapsLockState(false);
}

class PreferredReservedAcceleratorsTest : public AshTestBase {
 public:
  PreferredReservedAcceleratorsTest() = default;

  PreferredReservedAcceleratorsTest(const PreferredReservedAcceleratorsTest&) =
      delete;
  PreferredReservedAcceleratorsTest& operator=(
      const PreferredReservedAcceleratorsTest&) = delete;

  ~PreferredReservedAcceleratorsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->lock_state_controller()->set_animator_for_test(
        new TestSessionStateAnimator);
    Shell::Get()->power_button_controller()->OnGetSwitchStates(
        chromeos::PowerManagerClient::SwitchStates{
            chromeos::PowerManagerClient::LidState::OPEN,
            chromeos::PowerManagerClient::TabletMode::ON});
  }
};

TEST_F(PreferredReservedAcceleratorsTest, AcceleratorsWithFullscreen) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  aura::Window* w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  WMEvent fullscreen(WM_EVENT_FULLSCREEN);
  WindowState* w1_state = WindowState::Get(w1);
  w1_state->OnWMEvent(&fullscreen);
  ASSERT_TRUE(w1_state->IsFullscreen());

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Power key (reserved) should always be handled.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  PowerButtonControllerTestApi test_api(
      Shell::Get()->power_button_controller());
  EXPECT_FALSE(test_api.PowerButtonMenuTimerIsRunning());
  generator->PressKey(ui::VKEY_POWER, ui::EF_NONE);
  EXPECT_TRUE(test_api.PowerButtonMenuTimerIsRunning());

  auto press_and_release_alt_tab = [&generator]() {
    generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
    // Release the alt key to trigger the window activation.
    generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  };

  // A fullscreen window can consume ALT-TAB (preferred).
  ASSERT_EQ(w1, window_util::GetActiveWindow());
  press_and_release_alt_tab();
  ASSERT_EQ(w1, window_util::GetActiveWindow());
  ASSERT_NE(w2, window_util::GetActiveWindow());

  // ALT-TAB is non repeatable. Press A to cancel the
  // repeat record.
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // A normal window shouldn't consume preferred accelerator.
  WMEvent normal(WM_EVENT_NORMAL);
  w1_state->OnWMEvent(&normal);
  ASSERT_FALSE(w1_state->IsFullscreen());

  EXPECT_EQ(w1, window_util::GetActiveWindow());
  press_and_release_alt_tab();
  ASSERT_NE(w1, window_util::GetActiveWindow());
  ASSERT_EQ(w2, window_util::GetActiveWindow());
}

TEST_F(PreferredReservedAcceleratorsTest, AcceleratorsWithPinned) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  aura::Window* w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  {
    WMEvent pin_event(WM_EVENT_PIN);
    WindowState* w1_state = WindowState::Get(w1);
    w1_state->OnWMEvent(&pin_event);
    ASSERT_TRUE(w1_state->IsPinned());
  }

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Power key (reserved) should always be handled.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  PowerButtonControllerTestApi test_api(
      Shell::Get()->power_button_controller());
  EXPECT_FALSE(test_api.PowerButtonMenuTimerIsRunning());
  generator->PressKey(ui::VKEY_POWER, ui::EF_NONE);
  EXPECT_TRUE(test_api.PowerButtonMenuTimerIsRunning());

  // A pinned window can consume ALT-TAB (preferred), but no side effect.
  ASSERT_EQ(w1, window_util::GetActiveWindow());
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  ASSERT_EQ(w1, window_util::GetActiveWindow());
  ASSERT_NE(w2, window_util::GetActiveWindow());
}

TEST_F(AcceleratorControllerTest, DisallowedAtModalWindow) {
  std::set<AcceleratorAction> all_actions;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i)
    all_actions.insert(kAcceleratorData[i].action);
  std::set<AcceleratorAction> all_debug_actions;
  for (size_t i = 0; i < kDebugAcceleratorDataLength; ++i)
    all_debug_actions.insert(kDebugAcceleratorData[i].action);
  std::set<AcceleratorAction> all_dev_actions;
  for (size_t i = 0; i < kDeveloperAcceleratorDataLength; ++i)
    all_dev_actions.insert(kDeveloperAcceleratorData[i].action);

  std::set<AcceleratorAction> actionsAllowedAtModalWindow;
  for (size_t k = 0; k < kActionsAllowedAtModalWindowLength; ++k)
    actionsAllowedAtModalWindow.insert(kActionsAllowedAtModalWindow[k]);
  for (const auto& action : actionsAllowedAtModalWindow) {
    EXPECT_TRUE(base::Contains(all_actions, action) ||
                base::Contains(all_debug_actions, action) ||
                base::Contains(all_dev_actions, action))
        << " action from kActionsAllowedAtModalWindow"
        << " not found in kAcceleratorData, kDebugAcceleratorData or"
        << " kDeveloperAcceleratorData action: " << action;
  }
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window.get());
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  for (const auto& action : all_actions) {
    if (!base::Contains(actionsAllowedAtModalWindow, action)) {
      EXPECT_TRUE(controller_->PerformActionIfEnabled(action, {}))
          << " for action (disallowed at modal window): " << action;
    }
  }
  //  Testing of top row (F5-F10) accelerators that should still work
  //  when a modal window is open
  //
  // Screenshot
  auto* controller = CaptureModeController::Get();
  // Control + shift + F5 opens capture mode to take a region screenshot.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());
  controller->Stop();

  // Control + alt + F5 opens capture mode to take a window screenshot.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());
  controller->Stop();

  // Snapshot key opens capture mode with the last type, and closes it if it
  // is already open.
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_SNAPSHOT, ui::EF_NONE)));
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_SNAPSHOT, ui::EF_NONE)));
  ASSERT_FALSE(controller->IsActive());

  // Control + F5 takes a screenshot of all displays without opening capture
  // mode. The loop will timeout if a screenshot was not successfully taken
  // and saved.
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
  EXPECT_FALSE(controller->IsActive());
  base::RunLoop run_loop;
  CaptureModeTestApi().SetOnCaptureFileSavedCallback(base::BindLambdaForTesting(
      [&run_loop](const base::FilePath& path) { run_loop.Quit(); }));
  run_loop.Run();

  // Brightness
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate;
    SetBrightnessControlDelegate(
        std::unique_ptr<BrightnessControlDelegate>(delegate));
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessInController(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessInController(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  // Volume
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    base::UserActionTester user_action_tester;
    auto* history = controller_->GetAcceleratorHistory();

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_TRUE(ProcessInController(volume_mute));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_EQ(volume_mute, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_TRUE(ProcessInController(volume_down));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_EQ(volume_down, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
    EXPECT_TRUE(ProcessInController(volume_up));
    EXPECT_EQ(volume_up, history->current_accelerator());
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  }
}

TEST_F(AcceleratorControllerTest, DisallowedWithNoWindow) {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;

  // Extract the accelerators of actions that need windows to be able to provide
  // them to PerformActionIfEnabled(), otherwise we could hit some NOTREACHED()
  // if we don't provide the correct keybindings.
  std::set<AcceleratorAction> actions_needing_window;
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i)
    actions_needing_window.insert(kActionsNeedingWindow[i]);
  std::map<AcceleratorAction, ui::Accelerator> accelerators_needing_window;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i) {
    const auto& accelerator_data = kAcceleratorData[i];
    auto iter = actions_needing_window.find(accelerator_data.action);
    if (!base::Contains(actions_needing_window, accelerator_data.action)) {
      continue;
    }

    ui::Accelerator accelerator{accelerator_data.keycode,
                                accelerator_data.modifiers};
    if (!accelerator_data.trigger_on_press) {
      accelerator.set_key_state(ui::Accelerator::KeyState::RELEASED);
    }
    accelerators_needing_window[*iter] = accelerator;
  }

  for (const auto& iter : accelerators_needing_window) {
    accessibility_controller->TriggerAccessibilityAlert(
        AccessibilityAlert::NONE);
    EXPECT_TRUE(controller_->PerformActionIfEnabled(iter.first, iter.second));
    EXPECT_EQ(AccessibilityAlert::WINDOW_NEEDED, client.last_a11y_alert());
  }

  // Make sure we don't alert if we do have a window.
  std::unique_ptr<aura::Window> window;
  for (const auto& iter : accelerators_needing_window) {
    window.reset(CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
    wm::ActivateWindow(window.get());
    accessibility_controller->TriggerAccessibilityAlert(
        AccessibilityAlert::NONE);
    controller_->PerformActionIfEnabled(iter.first, iter.second);
    EXPECT_NE(AccessibilityAlert::WINDOW_NEEDED, client.last_a11y_alert());
  }

  // Don't alert if we have a minimized window either.
  for (const auto& iter : accelerators_needing_window) {
    window.reset(CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
    wm::ActivateWindow(window.get());
    controller_->PerformActionIfEnabled(AcceleratorAction::kWindowMinimize, {});
    accessibility_controller->TriggerAccessibilityAlert(
        AccessibilityAlert::NONE);
    controller_->PerformActionIfEnabled(iter.first, iter.second);
    EXPECT_NE(AccessibilityAlert::WINDOW_NEEDED, client.last_a11y_alert());
  }
}

TEST_F(AcceleratorControllerTest, TestDialogCancel) {
  ui::Accelerator accelerator(ui::VKEY_H,
                              ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Pressing cancel on the dialog should have no effect.
  EXPECT_FALSE(accessibility_controller->high_contrast().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(ProcessInController(accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  CancelConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(accessibility_controller->high_contrast().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());
}

TEST_F(AcceleratorControllerTest, TestToggleHighContrast) {
  ui::Accelerator accelerator(ui::VKEY_H,
                              ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
  // High Contrast Mode Enabled dialog and notification should be shown.
  EXPECT_FALSE(IsConfirmationDialogOpen());
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(accessibility_controller->high_contrast().WasDialogAccepted());
  EXPECT_TRUE(ProcessInController(accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  AcceptConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ContainsHighContrastNotification());
  EXPECT_FALSE(IsNotificationPinned(kHighContrastToggleAccelNotificationId));
  EXPECT_TRUE(accessibility_controller->high_contrast().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());

  // High Contrast Mode Enabled dialog and notification should be hidden as the
  // feature is disabled.
  EXPECT_TRUE(ProcessInController(accelerator));
  EXPECT_FALSE(ContainsHighContrastNotification());
  EXPECT_TRUE(accessibility_controller->high_contrast().WasDialogAccepted());

  // Notification should be shown again when toggled, but dialog will not be
  // shown.
  EXPECT_TRUE(ProcessInController(accelerator));
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(ContainsHighContrastNotification());
  RemoveAllNotifications();
}

TEST_F(AcceleratorControllerTest, CalculatorKey) {
  auto observer = std::make_unique<MockAcceleratorObserver>();
  auto* accelerator_controller = ash::AcceleratorController::Get();
  accelerator_controller->AddObserver(observer.get());

  // Verify that the launch calculator key (VKEY_MEDIA_LAUNCH_APP2) is
  // registered.
  ui::Accelerator accelerator(ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE);
  EXPECT_TRUE(controller_->IsRegistered(accelerator));

  // Verify that the delegate to open the app is called.
  EXPECT_CALL(new_window_delegate(), OpenCalculator)
      .WillOnce(testing::Return());
  EXPECT_CALL(*observer, OnActionPerformed)
      .WillOnce([](AcceleratorAction action) {
        EXPECT_EQ(AcceleratorAction::kOpenCalculator, action);
      });
  EXPECT_TRUE(ProcessInController(accelerator));
  accelerator_controller->RemoveObserver(observer.get());
}

// Tests the IME mode change key.
TEST_F(AcceleratorControllerTest, ChangeIMEMode_SwitchesInputMethod) {
  AddTestImes();

  ImeController* controller = Shell::Get()->ime_controller();

  TestImeControllerClient client;
  controller->SetClient(&client);

  EXPECT_EQ(0, client.next_ime_count_);

  ProcessInController(ui::Accelerator(ui::VKEY_MODECHANGE, ui::EF_NONE));

  EXPECT_EQ(1, client.next_ime_count_);
}

class SystemShortcutBehaviorTest : public AcceleratorControllerTest {
  void SetUp() override {
    AcceleratorControllerTest::SetUp();

    auto* session_controller = GetSessionControllerClient();

    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_prefs_ = user_prefs.get();
    RegisterUserProfilePrefs(user_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession(kUserEmail,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(AccountId::FromUserEmail(kUserEmail),
                                           std::move(user_prefs));
    SimulateUserLogin(AccountId::FromUserEmail(kUserEmail));
  }

  void TearDown() override {
    user_prefs_ = nullptr;
    AcceleratorControllerTest::TearDown();
  }

 protected:
  raw_ptr<TestingPrefServiceSimple> user_prefs_ = nullptr;
};

TEST_F(SystemShortcutBehaviorTest, StandardSearchBasedAcceleratorProcessing) {
  VoidEventHandler event_handler;
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  w1->AddPostTargetHandler(&event_handler);
  wm::ActivateWindow(w1);

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Generates 4 events, but the Search + D event gets consumed since it is a
  // valid keyboard shortcut.
  generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D, ui::EF_COMMAND_DOWN);

  // Since we generate 4 events, but one gets eaten, we expect 3 at the end of
  // this test.
  EXPECT_EQ(3, event_handler.num_events_received());
}

TEST_F(SystemShortcutBehaviorTest, IgnoreCommonVdiShortcuts) {
  user_prefs_->SetManagedPref(
      ash::prefs::kSystemShortcutBehavior,
      base::Value(static_cast<int>(
          SystemShortcutBehaviorType::kIgnoreCommonVdiShortcuts)));
  ui::Accelerator press_d_and_search(ui::VKEY_D, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(ProcessInController(press_d_and_search));

  user_prefs_->RemoveManagedPref(ash::prefs::kSystemShortcutBehavior);
  EXPECT_TRUE(ProcessInController(press_d_and_search));
}

TEST_F(SystemShortcutBehaviorTest, IgnoreCommonVdiShortcutsFullscreenOnly) {
  VoidEventHandler event_handler;
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  w1->AddPostTargetHandler(&event_handler);
  wm::ActivateWindow(w1);

  ui::test::EventGenerator* generator = GetEventGenerator();

  user_prefs_->SetManagedPref(
      ash::prefs::kSystemShortcutBehavior,
      base::Value(
          static_cast<int>(SystemShortcutBehaviorType::
                               kIgnoreCommonVdiShortcutsFullscreenOnly)));
  {
    // Generates 4 events, Search + D event does get consumed because the target
    // window is not fullscreen.
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    // Since we generate 4 events, but one gets eaten, we expect 3 at the end of
    // this test.
    EXPECT_EQ(3, event_handler.num_events_received());
    event_handler.ResetEventCounter();
  }

  // Make the window fullscreen.
  WMEvent fullscreen(WM_EVENT_FULLSCREEN);
  WindowState* w1_state = WindowState::Get(w1);
  w1_state->OnWMEvent(&fullscreen);
  ASSERT_TRUE(w1_state->IsFullscreen());

  // Tests that while fullscreen all events flow through since Search + D is in
  // the common VDI shortcut list.
  {
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    EXPECT_EQ(4, event_handler.num_events_received());
    event_handler.ResetEventCounter();
  }

  // Take out of fullscreen to verify the shortcuts still work.
  WMEvent normal(WM_EVENT_NORMAL);
  w1_state->OnWMEvent(&normal);
  ASSERT_FALSE(w1_state->IsFullscreen());

  // Tests that once the window is not fullscreen again, the event gets
  // consumed.
  {
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    EXPECT_EQ(3, event_handler.num_events_received());
  }
}

TEST_F(SystemShortcutBehaviorTest, AllowSearchBasedPassthrough) {
  VoidEventHandler event_handler;
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  w1->AddPostTargetHandler(&event_handler);
  wm::ActivateWindow(w1);

  ui::test::EventGenerator* generator = GetEventGenerator();

  user_prefs_->SetManagedPref(
      ash::prefs::kSystemShortcutBehavior,
      base::Value(static_cast<int>(
          SystemShortcutBehaviorType::kAllowSearchBasedPassthrough)));
  {
    // Generates 4 events, Search + D event does _not_ get consumed because we
    // are allowing search based events to flow through.
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    // Since we generate 4 events, but one gets eaten, we expect 3 at the end of
    // this test.
    EXPECT_EQ(4, event_handler.num_events_received());
    event_handler.ResetEventCounter();
  }

  user_prefs_->RemoveManagedPref(ash::prefs::kSystemShortcutBehavior);
  {
    // Generates 4 events, but the Search + D event gets consumed since it is a
    // valid keyboard shortcut.
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);

    // Since we generate 4 events, but one gets eaten, we expect 3 at the end of
    // this test.
    EXPECT_EQ(3, event_handler.num_events_received());
  }
}

TEST_F(SystemShortcutBehaviorTest, AllowSearchBasedPassthroughFullscreenOnly) {
  VoidEventHandler event_handler;
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  w1->AddPostTargetHandler(&event_handler);
  wm::ActivateWindow(w1);

  ui::test::EventGenerator* generator = GetEventGenerator();

  user_prefs_->SetManagedPref(
      ash::prefs::kSystemShortcutBehavior,
      base::Value(
          static_cast<int>(SystemShortcutBehaviorType::
                               kAllowSearchBasedPassthroughFullscreenOnly)));
  {
    // Generates 4 events, Search + D event does get consumed because the target
    // window is not fullscreen.
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    // Since we generate 4 events, but one gets eaten, we expect 3 at the end of
    // this test.
    EXPECT_EQ(3, event_handler.num_events_received());
    event_handler.ResetEventCounter();
  }

  // Make the window fullscreen.
  WMEvent fullscreen(WM_EVENT_FULLSCREEN);
  WindowState* w1_state = WindowState::Get(w1);
  w1_state->OnWMEvent(&fullscreen);
  ASSERT_TRUE(w1_state->IsFullscreen());

  // Tests that while fullscreen all events flow through.
  {
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    EXPECT_EQ(4, event_handler.num_events_received());
    event_handler.ResetEventCounter();
  }

  // Take out of fullscreen to verify the shortcuts still work.
  WMEvent normal(WM_EVENT_NORMAL);
  w1_state->OnWMEvent(&normal);
  ASSERT_FALSE(w1_state->IsFullscreen());

  // Tests that once the window is not fullscreen again, the event gets
  // consumed.
  {
    generator->PressAndReleaseKeyAndModifierKeys(ui::VKEY_D,
                                                 ui::EF_COMMAND_DOWN);
    EXPECT_EQ(3, event_handler.num_events_received());
  }
}

class AcceleratorControllerImprovedKeyboardShortcutsTest
    : public AcceleratorControllerTest {
 public:
  AcceleratorControllerImprovedKeyboardShortcutsTest() = default;
  ~AcceleratorControllerImprovedKeyboardShortcutsTest() override = default;

  class TestInputMethodManager : public input_method::MockInputMethodManager {
   public:
    void AddObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.AddObserver(observer);
    }

    void RemoveObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.RemoveObserver(observer);
    }

    // Calls all observers with Observer::InputMethodChanged
    void NotifyInputMethodChanged() {
      for (auto& observer : observers_) {
        observer.InputMethodChanged(
            /*manager=*/this, /*profile=*/nullptr, /*show_message=*/false);
      }
    }

    bool ArePositionalShortcutsUsedByCurrentInputMethod() const override {
      return use_positional_shortcuts_;
    }

    input_method::ImeKeyboard* GetImeKeyboard() override {
      return &ime_keyboard_;
    }

    input_method::FakeImeKeyboard ime_keyboard_;
    base::ObserverList<InputMethodManager::Observer>::Unchecked observers_;
    bool use_positional_shortcuts_ = false;
  };

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kImprovedKeyboardShortcuts);

    // Setup our own |InputMethodManager| to test that the accelerator
    // controller respects ArePositionalShortcutsUsedByCurrentInputMethod value
    // from the |InputMethodManager|.
    input_method_manager_ = new TestInputMethodManager();
    input_method::InputMethodManager::Initialize(input_method_manager_);

    AcceleratorControllerTest::SetUp();
    EXPECT_TRUE(
        input_method_manager_->observers_.HasObserver(controller_.get()));
  }

  void TearDown() override {
    AcceleratorControllerTest::TearDown();
    EXPECT_FALSE(
        input_method_manager_->observers_.HasObserver(controller_.get()));

    input_method::InputMethodManager::Shutdown();
    input_method_manager_ = nullptr;
  }

 protected:
  raw_ptr<TestInputMethodManager, DanglingUntriaged> input_method_manager_ =
      nullptr;  // Not owned.

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AcceleratorControllerImprovedKeyboardShortcutsTest, InputMethodChanged) {
  // Accelerator for Alt + Left Bracket on DE layout.
  const ui::Accelerator accelerator = ui::Accelerator(
      ui::VKEY_OEM_1, ui::DomCode::BRACKET_LEFT, ui::EF_ALT_DOWN);

  // With positional shortcuts disabled, the accelerator should not match
  // AcceleratorAction::kWindowCycleSnapLeft.
  input_method_manager_->use_positional_shortcuts_ = false;
  input_method_manager_->NotifyInputMethodChanged();
  EXPECT_FALSE(controller_->DoesAcceleratorMatchAction(
      accelerator, AcceleratorAction::kWindowCycleSnapLeft));

  // When enabled, accelerator should match
  // AcceleratorAction::kWindowCycleSnapLeft.
  input_method_manager_->use_positional_shortcuts_ = true;
  input_method_manager_->NotifyInputMethodChanged();
  EXPECT_TRUE(controller_->DoesAcceleratorMatchAction(
      accelerator, AcceleratorAction::kWindowCycleSnapLeft));
}

class AcceleratorControllerInputMethodTest : public AcceleratorControllerTest {
 public:
  AcceleratorControllerInputMethodTest() = default;
  ~AcceleratorControllerInputMethodTest() override = default;

  class AcceleratorMockInputMethod : public ui::MockInputMethod {
   public:
    AcceleratorMockInputMethod() : ui::MockInputMethod(nullptr) {}
    void CancelComposition(const ui::TextInputClient* client) override {
      cancel_composition_call_count++;
    }

    uint32_t cancel_composition_call_count = 0;
  };

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kImprovedKeyboardShortcuts);

    // Setup the mock input method to capture the calls to
    // |CancelCompositionAfterAccelerator|. Ownersship is passed to
    // ui::SetUpInputMethodForTesting().
    mock_input_ = new AcceleratorMockInputMethod();
    ui::SetUpInputMethodForTesting(mock_input_);
    AcceleratorControllerTest::SetUp();
  }

 protected:
  raw_ptr<AcceleratorMockInputMethod, DanglingUntriaged> mock_input_ =
      nullptr;  // Not owned.

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// In some layouts positional accelerators can be on dead/compose keys. To
// ensure that the input method is not left in a partially composed state
// the composition state is reset when an accelerator is matched.
TEST_F(AcceleratorControllerInputMethodTest, AcceleratorClearsComposition) {
  EXPECT_EQ(0u, mock_input_->cancel_composition_call_count);

  // An acclerator that isn't recognized will not cause composition to be
  // cancelled.
  ui::Accelerator unknown_accelerator(ui::VKEY_OEM_MINUS, ui::EF_NONE);
  EXPECT_FALSE(controller_->IsRegistered(unknown_accelerator));
  EXPECT_FALSE(ProcessInController(unknown_accelerator));
  EXPECT_EQ(0u, mock_input_->cancel_composition_call_count);

  // A matching accelerator should cause CancelCompositionAfterAccelerator() to
  // be called.
  ui::Accelerator accelerator(ui::VKEY_OEM_MINUS,
                              ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(controller_->IsRegistered(accelerator));
  EXPECT_TRUE(ProcessInController(accelerator));
  EXPECT_EQ(1u, mock_input_->cancel_composition_call_count);
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
class AcceleratorControllerDeprecatedTest : public AcceleratorControllerTest {
 public:
  AcceleratorControllerDeprecatedTest() = default;
  ~AcceleratorControllerDeprecatedTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kImprovedKeyboardShortcuts);
    AcceleratorControllerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_F(AcceleratorControllerDeprecatedTest, DeskShortcuts_Old) {
  // The shortcuts are Search+Shift+[MINUS|PLUS], but due to event
  // rewriting they became Shift+[F11|F12]. So only the rewritten shortcut
  // works but the "real" shortcut doesn't.
  EXPECT_TRUE(controller_->IsRegistered(
      ui::Accelerator(ui::VKEY_F12, ui::EF_SHIFT_DOWN)));
  EXPECT_TRUE(controller_->IsRegistered(
      ui::Accelerator(ui::VKEY_F11, ui::EF_SHIFT_DOWN)));
  EXPECT_FALSE(controller_->IsRegistered(ui::Accelerator(
      ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));
  EXPECT_FALSE(controller_->IsRegistered(ui::Accelerator(
      ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));
}

// defines a class to test the behavior of deprecated accelerators.
class DeprecatedAcceleratorTester : public AcceleratorControllerTest {
 public:
  DeprecatedAcceleratorTester() = default;

  DeprecatedAcceleratorTester(const DeprecatedAcceleratorTester&) = delete;
  DeprecatedAcceleratorTester& operator=(const DeprecatedAcceleratorTester&) =
      delete;

  ~DeprecatedAcceleratorTester() override = default;

  ui::Accelerator CreateAccelerator(const AcceleratorData& data) const {
    ui::Accelerator result(data.keycode, data.modifiers);
    result.set_key_state(data.trigger_on_press
                             ? ui::Accelerator::KeyState::PRESSED
                             : ui::Accelerator::KeyState::RELEASED);
    return result;
  }

  void ResetStateIfNeeded() {
    if (Shell::Get()->session_controller()->IsScreenLocked() ||
        Shell::Get()->session_controller()->IsUserSessionBlocked()) {
      UnblockUserSession();
    }
  }

  bool ContainsDeprecatedAcceleratorNotification(const char* const id) const {
    return nullptr != message_center()->FindVisibleNotificationById(id);
  }

  bool IsMessageCenterEmpty() const {
    return message_center()->GetVisibleNotifications().empty();
  }
};

TEST_F(DeprecatedAcceleratorTester, TestDeprecatedAcceleratorsBehavior) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  ScopedDictPrefUpdate time_update(
      prefs, prefs::kDeprecatedAcceleratorNotificationsLastShown);

  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& entry = kDeprecatedAccelerators[i];

    const DeprecatedAcceleratorData* data =
        test_api_->GetDeprecatedAcceleratorData(entry.action);
    DCHECK(data);

    EXPECT_TRUE(IsMessageCenterEmpty());
    ui::Accelerator deprecated_accelerator = CreateAccelerator(entry);

    time_update->Set(data->pref_name,
                     base::TimeToValue(base::Time::Now() - base::Hours(24)));
    if (data->deprecated_enabled)
      EXPECT_TRUE(ProcessInController(deprecated_accelerator));
    else
      EXPECT_FALSE(ProcessInController(deprecated_accelerator));

    // We expect to see a notification in the message center.
    EXPECT_TRUE(
        ContainsDeprecatedAcceleratorNotification(data->uma_histogram_name));
    RemoveAllNotifications();

    // If the action is AcceleratorAction::kLockScreen, we must reset the state
    // by unlocking the screen before we proceed testing the rest of
    // accelerators.
    ResetStateIfNeeded();
  }
}

TEST_F(DeprecatedAcceleratorTester, NoNotificationIfReplacementMissing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kShortcutCustomization);

  // Remove the replacements for all deprecated accelerators.
  Shell::Get()->ash_accelerator_configuration()->RemoveAccelerator(
      AcceleratorAction::kShowShortcutViewer,
      ui::Accelerator{ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN});

  Shell::Get()->ash_accelerator_configuration()->RemoveAccelerator(
      AcceleratorAction::kOpenGetHelp,
      ui::Accelerator{ui::VKEY_H, ui::EF_COMMAND_DOWN});

  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& entry = kDeprecatedAccelerators[i];

    const DeprecatedAcceleratorData* data =
        test_api_->GetDeprecatedAcceleratorData(entry.action);
    DCHECK(data);

    EXPECT_TRUE(IsMessageCenterEmpty());
    ui::Accelerator deprecated_accelerator = CreateAccelerator(entry);
    if (data->deprecated_enabled) {
      EXPECT_TRUE(ProcessInController(deprecated_accelerator));
    } else {
      EXPECT_FALSE(ProcessInController(deprecated_accelerator));
    }

    // We do not expect to see a notification in the message center.
    EXPECT_FALSE(
        ContainsDeprecatedAcceleratorNotification(data->uma_histogram_name));

    ResetStateIfNeeded();
  }
}

TEST_F(DeprecatedAcceleratorTester, TestNewAccelerators) {
  // Add below the new accelerators that replaced the deprecated ones (if any).
  const AcceleratorData kNewAccelerators[] = {
      {true, ui::VKEY_L, ui::EF_COMMAND_DOWN, AcceleratorAction::kLockScreen},
      {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kSwitchToNextIme},
      {true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      {true, ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleImeMenuBubble},
      {true, ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleHighContrast},
  };

  // The AcceleratorAction::kSwitchToNextIme accelerator requires multiple IMEs
  // to be available.
  AddTestImes();

  EXPECT_TRUE(IsMessageCenterEmpty());

  for (auto data : kNewAccelerators) {
    EXPECT_TRUE(ProcessInController(CreateAccelerator(data)));

    // Expect no notifications from the new accelerators.
    if (data.action != AcceleratorAction::kToggleHighContrast) {
      // The toggle high contrast accelerator displays a notification specific
      // to the high contrast mode.
      EXPECT_TRUE(IsMessageCenterEmpty());
    }

    // If the action is AcceleratorAction::kLockScreen, we must reset the state
    // by unlocking the screen before we proceed testing the rest of
    // accelerators.
    ResetStateIfNeeded();
  }

  RemoveAllNotifications();
}

using AcceleratorControllerGuestModeTest = NoSessionAshTestBase;

TEST_F(AcceleratorControllerGuestModeTest, IncognitoWindowDisabled) {
  SimulateGuestLogin();

  // New incognito window is disabled.
  EXPECT_FALSE(Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kNewIncognitoWindow, {}));
}

class MagnifiersAcceleratorsTester : public AcceleratorControllerTest {
 public:
  MagnifiersAcceleratorsTester() = default;

  MagnifiersAcceleratorsTester(const MagnifiersAcceleratorsTester&) = delete;
  MagnifiersAcceleratorsTester& operator=(const MagnifiersAcceleratorsTester&) =
      delete;

  ~MagnifiersAcceleratorsTester() override = default;

  DockedMagnifierController* docked_magnifier_controller() const {
    return Shell::Get()->docked_magnifier_controller();
  }

  FullscreenMagnifierController* fullscreen_magnifier_controller() const {
    return Shell::Get()->fullscreen_magnifier_controller();
  }

  PrefService* user_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUserEmail));
  }

  void SetUp() override {
    AcceleratorControllerTest::SetUp();
    feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMagnifyAcceleratorDialog);

    // Create user session and simulate its login.
    SimulateUserLogin(kUserEmail);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO (afakhry): Remove this class after refactoring MagnificationManager.
// Mocked chrome/browser/ash/accessibility/magnification_manager.cc
class FakeMagnificationManager {
 public:
  FakeMagnificationManager() = default;

  FakeMagnificationManager(const FakeMagnificationManager&) = delete;
  FakeMagnificationManager& operator=(const FakeMagnificationManager&) = delete;

  void SetPrefs(PrefService* prefs) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(prefs);
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierEnabled,
        base::BindRepeating(&FakeMagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    prefs_ = prefs;
  }

  void UpdateMagnifierFromPrefs() {
    Shell::Get()->fullscreen_magnifier_controller()->SetEnabled(
        prefs_->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled));
  }

 private:
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> prefs_;
};

TEST_F(MagnifiersAcceleratorsTester, TestToggleFullscreenMagnifier) {
  FakeMagnificationManager manager;
  manager.SetPrefs(user_pref_service());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_FALSE(IsConfirmationDialogOpen());

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Toggle the fullscreen magnifier on/off, dialog should be shown on first use
  // of accelerator.
  const ui::Accelerator fullscreen_magnifier_accelerator(
      ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  // Magnifier is enabled in order to let users better see the dialog.
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());

  CancelConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  // Magnifier is disabled when the dialog is cancelled.
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());

  // Open the dialog again.
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());

  AcceptConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsFullscreenMagnifierNotification());
  EXPECT_FALSE(
      IsNotificationPinned(kFullscreenMagnifierToggleAccelNotificationId));

  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_FALSE(ContainsFullscreenMagnifierNotification());

  // Dialog will not be shown the second time the accelerator is used.
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsFullscreenMagnifierNotification());

  RemoveAllNotifications();
}

TEST_F(MagnifiersAcceleratorsTester, TestToggleDockedMagnifier) {
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_FALSE(IsConfirmationDialogOpen());

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Toggle the docked magnifier on/off, dialog should be shown on first use of
  // accelerator.
  const ui::Accelerator docked_magnifier_accelerator(
      ui::VKEY_D, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(ProcessInController(docked_magnifier_accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  AcceptConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsDockedMagnifierNotification());
  EXPECT_FALSE(IsNotificationPinned(kDockedMagnifierToggleAccelNotificationId));

  EXPECT_TRUE(ProcessInController(docked_magnifier_accelerator));
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(accessibility_controller->docked_magnifier().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_FALSE(ContainsDockedMagnifierNotification());

  // Dialog will not be shown the second time accelerator is used.
  EXPECT_TRUE(ProcessInController(docked_magnifier_accelerator));
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsDockedMagnifierNotification());

  RemoveAllNotifications();
}

class MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest
    : public MagnifiersAcceleratorsTester {
 public:
  MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest() = default;
  MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest(
      const MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest&) =
      delete;
  MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest& operator=(
      const MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest&) =
      delete;
  ~MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest() override =
      default;

  void SetUp() override {
    AcceleratorControllerTest::SetUp();
    feature_list_.InitAndDisableFeature(
        ::features::kAccessibilityMagnifyAcceleratorDialog);

    // Create user session and simulate its login.
    SimulateUserLogin(kUserEmail);
  }
};

TEST_F(MagnifiersAcceleratorsMagnifyAcceleratorDialogDisabledTest,
       TestToggleFullscreenMagnifier) {
  FakeMagnificationManager manager;
  manager.SetPrefs(user_pref_service());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_FALSE(IsConfirmationDialogOpen());

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Toggle the fullscreen magnifier on/off, dialog should be shown on first use
  // of accelerator.
  const ui::Accelerator fullscreen_magnifier_accelerator(
      ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  // Magnifier is not enabled when feature is not on.
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());

  CancelConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());

  // Open the dialog again.
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_TRUE(IsConfirmationDialogOpen());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());

  AcceptConfirmationDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsFullscreenMagnifierNotification());
  EXPECT_FALSE(
      IsNotificationPinned(kFullscreenMagnifierToggleAccelNotificationId));

  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_FALSE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_FALSE(ContainsFullscreenMagnifierNotification());

  // Dialog will not be shown the second time the accelerator is used.
  EXPECT_TRUE(ProcessInController(fullscreen_magnifier_accelerator));
  EXPECT_FALSE(IsConfirmationDialogOpen());
  EXPECT_TRUE(
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted());
  EXPECT_FALSE(docked_magnifier_controller()->GetEnabled());
  EXPECT_TRUE(fullscreen_magnifier_controller()->IsEnabled());
  EXPECT_TRUE(ContainsFullscreenMagnifierNotification());

  RemoveAllNotifications();
}

class AccessibilityAcceleratorTester : public MagnifiersAcceleratorsTester {
 public:
  AccessibilityAcceleratorTester() = default;
  ~AccessibilityAcceleratorTester() override = default;

  AccessibilityAcceleratorTester(const AccessibilityAcceleratorTester&) =
      delete;
  AccessibilityAcceleratorTester& operator=(
      const AccessibilityAcceleratorTester&) = delete;

  bool ContainsAccessibilityNotification(
      const std::string& notification_id) const {
    return nullptr !=
           message_center()->FindVisibleNotificationById(notification_id);
  }

  void TestAccessibilityAcceleratorControlledByPref(
      const std::string& pref_name,
      const char* notification_id,
      const ui::Accelerator& accelerator) {
    // Verify that the initial state for the accessibility feature will be
    // disabled, and for accessibility accelerators controller pref
    // |kAccessibilityShortcutsEnabled| is enabled. And neither of that
    // accessibility feature notification id, nor its confirmation dialog have
    // appeared.
    base::HistogramTester histogram_tester_;

    EXPECT_FALSE(user_pref_service()->GetBoolean(pref_name));
    EXPECT_TRUE(
        user_pref_service()->GetBoolean(prefs::kAccessibilityShortcutsEnabled));
    EXPECT_FALSE(IsConfirmationDialogOpen());
    if (notification_id)
      EXPECT_FALSE(ContainsAccessibilityNotification(notification_id));

    // Verify that after disabling the accessibility accelerators, the
    // confirmation dialog won't appear for that accessibility feature. And its
    // corresponding pref won't be enabled. But a notification should appear,
    // which shows that the shortcut for that feature has been disabled. And
    // verify that the accessibility shortcut state is being recorded
    // accordingly.
    user_pref_service()->SetBoolean(prefs::kAccessibilityShortcutsEnabled,
                                    false);
    EXPECT_TRUE(ProcessInController(accelerator));
    EXPECT_FALSE(IsConfirmationDialogOpen());
    if (notification_id)
      EXPECT_TRUE(ContainsAccessibilityNotification(notification_id));
    EXPECT_FALSE(user_pref_service()->GetBoolean(pref_name));

    // Verify that if the accessibility accelerators are enabled, then
    // it will show the confirmation dialog for the first time only when
    // toggling its value. And the coressponding pref will be chanaged
    // accordingly. And verify that the accessibility shortcut state is being
    // recorded accordingly.
    user_pref_service()->SetBoolean(prefs::kAccessibilityShortcutsEnabled,
                                    true);
    EXPECT_TRUE(ProcessInController(accelerator));
    if (notification_id)
      AcceptConfirmationDialog();
    base::RunLoop().RunUntilIdle();
    message_center::NotificationList::Notifications notifications =
        message_center()->GetVisibleNotifications();
    ASSERT_EQ(1u, notifications.size());
    EXPECT_TRUE(user_pref_service()->GetBoolean(pref_name));
    if (notification_id)
      EXPECT_TRUE(ContainsAccessibilityNotification(notification_id));

    // Verify that the notification id, won't be shown if the accessibility
    // feature is going to be disabled. And verify that the accessibility
    // shortcut state is being recorded accordingly.
    EXPECT_TRUE(ProcessInController(accelerator));
    if (notification_id)
      EXPECT_FALSE(ContainsAccessibilityNotification(notification_id));
    EXPECT_FALSE(user_pref_service()->GetBoolean(pref_name));

    // Remove all the current notifications, to get the initial state again.
    RemoveAllNotifications();
  }
};

TEST_F(AccessibilityAcceleratorTester, DisableAccessibilityAccelerators) {
  FakeMagnificationManager manager;
  manager.SetPrefs(user_pref_service());
  for (const auto& test_data : kAccessibilityAcceleratorMap) {
    TestAccessibilityAcceleratorControlledByPref(
        test_data.pref_name, test_data.notification_id, test_data.accelerator);
  }
}

// Tests that the shortcuts for starting another screen capture session will be
// treated as no-op if a capture session is already running.
TEST_F(AccessibilityAcceleratorTester,
       DisableScreenCaptureAcceleratorsIfSessionIsActive) {
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());

  // Start a window capture session.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());

  //  Accelerators for partial screenshot will be a no-op if a
  //  session is already running.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());

  controller->Stop();

  // Start a partial screenshot capture session.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());

  //  Accelerators for window screenshot will be a no-op if a
  //  session is already running.
  EXPECT_TRUE(ProcessInController(ui::Accelerator(
      ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());
}

struct MediaSessionAcceleratorTestConfig {
  // Runs the test with the media session service enabled.
  bool service_enabled;

  // Runs the test with the supplied action enabled and will also send the media
  // session info to the controller.
  std::optional<MediaSessionAction> with_action_enabled;

  // If true then we should expect the action will handle the media keys.
  bool eligible_action = false;

  // If true then we should force forwarding the action to the client.
  bool force_key_handling = false;
};

}  // namespace

// MediaSessionAcceleratorTest tests media key handling with media session
// service integration. The parameter is a struct that configures different
// settings to run the test under.
// Note this class can't be in the anonymous namespace because it is referenced
// as a friend by ash/media/media_controller_impl.h.
class MediaSessionAcceleratorTest
    : public AcceleratorControllerTest,
      public testing::WithParamInterface<MediaSessionAcceleratorTestConfig> {
 public:
  MediaSessionAcceleratorTest() = default;

  MediaSessionAcceleratorTest(const MediaSessionAcceleratorTest&) = delete;
  MediaSessionAcceleratorTest& operator=(const MediaSessionAcceleratorTest&) =
      delete;

  ~MediaSessionAcceleratorTest() override = default;

  // AcceleratorControllerTest:
  void SetUp() override {
    if (service_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          media::kHardwareMediaKeyHandling);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          media::kHardwareMediaKeyHandling);
    }

    AcceleratorControllerTest::SetUp();

    client_ = std::make_unique<TestMediaClient>();
    controller_ = std::make_unique<media_session::test::TestMediaController>();

    MediaControllerImpl* media_controller = Shell::Get()->media_controller();
    media_controller->SetClient(client_.get());
    media_controller->SetMediaSessionControllerForTest(
        controller_->CreateMediaControllerRemote());
    media_controller->SetForceMediaClientKeyHandling(
        GetParam().force_key_handling);
    media_controller->FlushForTesting();
  }

  void MaybeEnableMediaSession(
      media_session::mojom::MediaPlaybackState playback_state) {
    if (!GetParam().with_action_enabled)
      return;

    SimulateActionsChanged(GetParam().with_action_enabled);
    SimulatePlaybackState(playback_state);
  }

  void SimulateActionsChanged(std::optional<MediaSessionAction> action) {
    std::vector<MediaSessionAction> actions;

    if (action)
      actions.push_back(*action);

    controller()->SimulateMediaSessionActionsChanged(actions);
    controller()->Flush();
  }

  TestMediaClient* client() const { return client_.get(); }

  media_session::test::TestMediaController* controller() const {
    return controller_.get();
  }

  bool service_enabled() const { return GetParam().service_enabled; }

  bool eligible_action() const { return GetParam().eligible_action; }

  bool force_key_handling() const { return GetParam().force_key_handling; }

  void ExpectActionRecorded(ui::MediaHardwareKeyAction action) {
    histogram_tester_.ExpectBucketCount(
        ui::kMediaHardwareKeyActionHistogramName,
        static_cast<base::HistogramBase::Sample>(action), 1);
  }

 private:
  void SimulatePlaybackState(
      media_session::mojom::MediaPlaybackState playback_state) {
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());

    session_info->state =
        media_session::mojom::MediaSessionInfo::SessionState::kActive;
    session_info->playback_state = playback_state;

    controller()->SimulateMediaSessionInfoChanged(std::move(session_info));
    controller()->Flush();
  }

  std::unique_ptr<TestMediaClient> client_;
  std::unique_ptr<media_session::test::TestMediaController> controller_;

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaSessionAcceleratorTest,
    testing::Values(
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kPlay,
                                          true},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kPause,
                                          true},
        MediaSessionAcceleratorTestConfig{
            true, MediaSessionAction::kPreviousTrack, true},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kNextTrack,
                                          true},
        MediaSessionAcceleratorTestConfig{true,
                                          MediaSessionAction::kSeekBackward},
        MediaSessionAcceleratorTestConfig{true,
                                          MediaSessionAction::kSeekForward},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kStop},
        MediaSessionAcceleratorTestConfig{false, MediaSessionAction::kPlay},
        MediaSessionAcceleratorTestConfig{false, MediaSessionAction::kPause},
        MediaSessionAcceleratorTestConfig{false,
                                          MediaSessionAction::kPreviousTrack},
        MediaSessionAcceleratorTestConfig{false,
                                          MediaSessionAction::kNextTrack},
        MediaSessionAcceleratorTestConfig{false,
                                          MediaSessionAction::kSeekBackward},
        MediaSessionAcceleratorTestConfig{false,
                                          MediaSessionAction::kSeekForward},
        MediaSessionAcceleratorTestConfig{false, MediaSessionAction::kStop},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kPlay,
                                          false, true},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kPause,
                                          false, true},
        MediaSessionAcceleratorTestConfig{true, MediaSessionAction::kNextTrack,
                                          false, true},
        MediaSessionAcceleratorTestConfig{
            true, MediaSessionAction::kPreviousTrack, false, true}));

TEST_P(MediaSessionAcceleratorTest, MediaPlaybackAcceleratorsBehavior) {
  const ui::KeyboardCode media_keys[] = {ui::VKEY_MEDIA_NEXT_TRACK,
                                         ui::VKEY_MEDIA_PLAY_PAUSE,
                                         ui::VKEY_MEDIA_PREV_TRACK};
  ::wm::AcceleratorFilter filter(
      std::make_unique<PreTargetAcceleratorHandler>());

  for (ui::KeyboardCode key : media_keys) {
    // If the media session service integration is enabled then media keys will
    // be handled in ash.
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
    {
      ui::KeyEvent press_key(ui::EventType::kKeyPressed, key, ui::EF_NONE);
      ui::Event::DispatcherApi dispatch_helper(&press_key);
      dispatch_helper.set_target(window.get());
      filter.OnKeyEvent(&press_key);
      EXPECT_EQ(service_enabled(), press_key.stopped_propagation());
    }

    // Setting a window property on the target allows media keys to pass
    // through.
    WindowState::Get(window.get())->SetCanConsumeSystemKeys(true);
    {
      ui::KeyEvent press_key(ui::EventType::kKeyPressed, key, ui::EF_NONE);
      ui::Event::DispatcherApi dispatch_helper(&press_key);
      dispatch_helper.set_target(window.get());
      filter.OnKeyEvent(&press_key);
      EXPECT_FALSE(press_key.stopped_propagation());
    }
  }
}

TEST_P(MediaSessionAcceleratorTest, MediaGlobalAccelerators_NextTrack) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_EQ(0, client()->handle_media_next_track_count());
  EXPECT_EQ(0, controller()->next_track_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(0, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else {
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }

  ExpectActionRecorded(ui::MediaHardwareKeyAction::kNextTrack);
}

TEST_P(MediaSessionAcceleratorTest, MediaGlobalAccelerators_Play) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_EQ(0, client()->handle_media_play_pause_count());
  EXPECT_EQ(0, controller()->resume_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action()) {
    EXPECT_EQ(0, client()->handle_media_play_pause_count());
    EXPECT_EQ(1, controller()->resume_count());

    // If media session handles the key then we should always know the playback
    // state so we can record a more granular action.
    ExpectActionRecorded(ui::MediaHardwareKeyAction::kPlay);
  } else {
    EXPECT_EQ(1, client()->handle_media_play_pause_count());
    EXPECT_EQ(0, controller()->resume_count());

    // If we pass through to the client we don't know whether the action will
    // play or pause so we should record a generic "play/pause" action.
    ExpectActionRecorded(ui::MediaHardwareKeyAction::kPlayPause);
  }
}

TEST_P(MediaSessionAcceleratorTest, MediaGlobalAccelerators_Pause) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_EQ(0, client()->handle_media_play_pause_count());
  EXPECT_EQ(0, controller()->suspend_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(0, client()->handle_media_play_pause_count());
    EXPECT_EQ(1, controller()->suspend_count());

    // If media session handles the key then we should always know the playback
    // state so we can record a more granular action.
    ExpectActionRecorded(ui::MediaHardwareKeyAction::kPause);
  } else {
    EXPECT_EQ(1, client()->handle_media_play_pause_count());
    EXPECT_EQ(0, controller()->suspend_count());

    // If we pass through to the client we don't know whether the action will
    // play or pause so we should record a generic "play/pause" action.
    ExpectActionRecorded(ui::MediaHardwareKeyAction::kPlayPause);
  }
}

TEST_P(MediaSessionAcceleratorTest, MediaGlobalAccelerators_PrevTrack) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_EQ(0, client()->handle_media_prev_track_count());
  EXPECT_EQ(0, controller()->previous_track_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(0, client()->handle_media_prev_track_count());
    EXPECT_EQ(1, controller()->previous_track_count());
  } else {
    EXPECT_EQ(1, client()->handle_media_prev_track_count());
    EXPECT_EQ(0, controller()->previous_track_count());
  }

  ExpectActionRecorded(ui::MediaHardwareKeyAction::kPreviousTrack);
}

TEST_P(MediaSessionAcceleratorTest,
       MediaGlobalAccelerators_UpdateAction_Disable) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_EQ(0, client()->handle_media_next_track_count());
  EXPECT_EQ(0, controller()->next_track_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(0, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else {
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }

  SimulateActionsChanged(std::nullopt);

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else {
    EXPECT_EQ(2, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }
}

TEST_P(MediaSessionAcceleratorTest,
       MediaGlobalAccelerators_UpdateAction_Enable) {
  EXPECT_EQ(0, client()->handle_media_next_track_count());
  EXPECT_EQ(0, controller()->next_track_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  EXPECT_EQ(1, client()->handle_media_next_track_count());
  EXPECT_EQ(0, controller()->next_track_count());

  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else {
    EXPECT_EQ(2, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }
}

TEST_P(MediaSessionAcceleratorTest,
       MediaGlobalAccelerators_UpdateForceKeyHandling) {
  MaybeEnableMediaSession(media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_EQ(0, client()->handle_media_next_track_count());
  EXPECT_EQ(0, controller()->next_track_count());

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && eligible_action() && !force_key_handling()) {
    EXPECT_EQ(0, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else {
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }

  // Update the force media client key handling setting. It may have been
  // previously set if |force_key_handling| is true.
  Shell::Get()->media_controller()->SetForceMediaClientKeyHandling(false);

  ProcessInController(ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE));
  Shell::Get()->media_controller()->FlushForTesting();

  if (service_enabled() && force_key_handling()) {
    // If we had |force_key_handling| true the first time we pressed the play
    // pause key then we should see the previous action that was forward to the
    // client. Since the service is enabled, the second action will be handled
    // by the controller.
    EXPECT_EQ(1, client()->handle_media_next_track_count());
    EXPECT_EQ(1, controller()->next_track_count());
  } else if (service_enabled() && eligible_action()) {
    // If we had |force_key_handling| disabled the whole time and the service
    // enabled then both actions should be handled in the controller.
    EXPECT_EQ(0, client()->handle_media_next_track_count());
    EXPECT_EQ(2, controller()->next_track_count());
  } else {
    // If we had |force_key_handling| disabled the whole time and the service
    // disabled then both actions should fallback to the client because there is
    // nothing in Ash to handle them.
    EXPECT_EQ(2, client()->handle_media_next_track_count());
    EXPECT_EQ(0, controller()->next_track_count());
  }
}

// TODO(b:332383246): Remove once the feature is enabled permanently.
class AcceleratorControllerGameDashboardTests
    : public AcceleratorControllerTest {
 public:
  AcceleratorControllerGameDashboardTests() = default;
  AcceleratorControllerGameDashboardTests(
      const AcceleratorControllerTestWithClamshellSplitView&) = delete;
  AcceleratorControllerGameDashboardTests& operator=(
      const AcceleratorControllerGameDashboardTests&) = delete;
  ~AcceleratorControllerGameDashboardTests() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGameDashboard);
    AcceleratorControllerTest::SetUp();
    EXPECT_TRUE(features::IsGameDashboardEnabled());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AcceleratorControllerGameDashboardTests,
       ToggleGameDashboardAccelerator) {
  const ui::Accelerator accelerator(ui::VKEY_G, ui::EF_COMMAND_DOWN);

  // No active window.
  EXPECT_FALSE(ProcessInController(accelerator));

  // Create an ARC app window.
  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), chromeos::AppType::ARC_APP);
  window->SetProperty(kAppIDKey,
                      std::string(TestGameDashboardDelegate::kGameAppId));
  // Verify the accelerator is not processed until the game controls status is
  // known for ARC apps.
  EXPECT_FALSE(ProcessInController(accelerator));
  window->SetProperty(kArcGameControlsFlagsKey, ArcGameControlsFlag::kKnown);
  EXPECT_TRUE(ProcessInController(accelerator));
  // Verify the accelerator is not processed when game controls is in edit mode.
  window->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kEdit));
  EXPECT_FALSE(ProcessInController(accelerator));

  // Create a non-ARC app window.
  window = CreateAppWindow(gfx::Rect(5, 5, 20, 20), chromeos::AppType::BROWSER);
  window->SetProperty(
      kAppIDKey, std::string(TestGameDashboardDelegate::kAllowlistedAppId));
  EXPECT_TRUE(ProcessInController(accelerator));
}

}  // namespace ash
