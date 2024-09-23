// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/status_area_internals/status_area_internals_handler.h"

#include <memory>
#include <string_view>

#include "ash/annotator/annotation_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/fake_power_status.h"
#include "ash/system/model/fake_system_tray_model.h"
#include "ash/system/model/scoped_fake_power_status.h"
#include "ash/system/model/scoped_fake_system_tray_model.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/quick_settings_header.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/status_area_internals/mojom/status_area_internals.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

namespace {

// A class that mocks `MagicBoostStateAsh` to use in tests.
class TestMagicBoostState : public chromeos::MagicBoostState {
 public:
  TestMagicBoostState() = default;

  TestMagicBoostState(const TestMagicBoostState&) = delete;
  TestMagicBoostState& operator=(const TestMagicBoostState&) = delete;

  ~TestMagicBoostState() override = default;

  // chromeos::MagicBoostState:
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override {
    UpdateHMRConsentStatus(consent_status);
  }

  bool IsMagicBoostAvailable() override { return true; }
  bool CanShowNoticeBannerForHMR() override { return false; }
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override { return 0; }
  void AsyncWriteHMREnabled(bool enabled) override {}
  void DisableOrcaFeature() override {}
};

}  // namespace

class StatusAreaInternalsHandlerTest : public AshTestBase {
 public:
  StatusAreaInternalsHandlerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  StatusAreaInternalsHandlerTest(const StatusAreaInternalsHandlerTest&) =
      delete;
  StatusAreaInternalsHandlerTest& operator=(
      const StatusAreaInternalsHandlerTest&) = delete;
  ~StatusAreaInternalsHandlerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    // Need to use test resources instead to have `AshTestBase` work on
    // //ash/webui.
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();

    AshTestBase::SetUp();

    handler_ = std::make_unique<StatusAreaInternalsHandler>(
        handler_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    handler_.reset();
    AshTestBase::TearDown();
  }

  FakeSystemTrayModel* GetFakeModel() {
    return handler_->scoped_fake_model_->fake_model();
  }

  StatusAreaWidget* GetStatusAreaWidget() {
    return ash::Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget();
  }

  const mojo::Remote<mojom::status_area_internals::PageHandler>&
  handler_remote() {
    return handler_remote_;
  }

  std::unique_ptr<StatusAreaInternalsHandler> handler_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;

  mojo::Remote<mojom::status_area_internals::PageHandler> handler_remote_;
};

// Tests toggle the visibility of tray buttons.
TEST_F(StatusAreaInternalsHandlerTest, ToggleTrayButtons) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kEnableStylusTools, true);

  struct ToggleTrayTestParam {
    std::string_view tray_name;

    // The current tray that is being tested.
    raw_ptr<TrayBackgroundView> tray;

    // The function that should toggle the visibility of the tested tray.
    base::RepeatingCallback<void(bool)> toggle_function;
  };

  const ToggleTrayTestParam test_cases[] = {
      // IME Tray
      ToggleTrayTestParam{
          "IME Tray", GetStatusAreaWidget()->ime_menu_tray(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::ToggleImeTray,
              base::Unretained(handler_remote().get()))},
      // Palette Tray
      ToggleTrayTestParam{
          "Palette Tray", GetStatusAreaWidget()->palette_tray(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::TogglePaletteTray,
              base::Unretained(handler_remote().get()))},
      // Logout Tray
      ToggleTrayTestParam{
          "Logout Tray",
          GetStatusAreaWidget()->logout_button_tray_for_testing(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::ToggleLogoutTray,
              base::Unretained(handler_remote().get()))},
      // Virtual Keyboard Tray
      ToggleTrayTestParam{
          "Virtual Keyboard Tray",
          GetStatusAreaWidget()->virtual_keyboard_tray_for_testing(),
          base::BindRepeating(&mojom::status_area_internals::PageHandler::
                                  ToggleVirtualKeyboardTray,
                              base::Unretained(handler_remote().get()))},
      // Dictation Tray
      ToggleTrayTestParam{
          "Dictation Tray", GetStatusAreaWidget()->dictation_button_tray(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::ToggleDictationTray,
              base::Unretained(handler_remote().get()))},
      // Video Conference Tray
      ToggleTrayTestParam{
          "Video Conference Tray",
          GetStatusAreaWidget()->video_conference_tray(),
          base::BindRepeating(&mojom::status_area_internals::PageHandler::
                                  ToggleVideoConferenceTray,
                              base::Unretained(handler_remote().get()))},
      // Annotation Tray
      ToggleTrayTestParam{
          "Annotation Tray", GetStatusAreaWidget()->annotation_tray(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::ToggleAnnotationTray,
              base::Unretained(handler_remote().get()))}};

  // Test that when triggering the correct `toggle_function` from the test web
  // UI remote, the tray should update the
  // visibility accordingly.
  for (auto& test_param : test_cases) {
    SCOPED_TRACE(test_param.tray_name);

    auto tray = test_param.tray;
    EXPECT_FALSE(tray->GetVisible());

    test_param.toggle_function.Run(/*visible=*/true);
    task_environment()->RunUntilIdle();

    EXPECT_TRUE(tray->GetVisible()) << test_param.tray_name;

    test_param.toggle_function.Run(/*visible=*/false);
    task_environment()->RunUntilIdle();

    EXPECT_FALSE(tray->GetVisible()) << test_param.tray_name;
  }
}

TEST_F(StatusAreaInternalsHandlerTest, SetIsInUserChildSession) {
  handler_remote()->SetIsInUserChildSession(/*in_child_session=*/true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(GetFakeModel()->IsInUserChildSession());

  // Make sure that the supervised UI is visible.
  LeftClickOn(GetPrimaryUnifiedSystemTray());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()
                  ->bubble()
                  ->quick_settings_view()
                  ->header_for_testing()
                  ->GetSupervisedButtonForTest()
                  ->GetVisible());

  // Close the quick settings bubble.
  LeftClickOn(GetPrimaryUnifiedSystemTray());

  // Test the reset case.
  handler_remote()->SetIsInUserChildSession(/*in_child_session=*/false);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(GetFakeModel()->IsInUserChildSession());

  // Make sure that the supervised UI is not visible.
  LeftClickOn(GetPrimaryUnifiedSystemTray());
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()
                   ->bubble()
                   ->quick_settings_view()
                   ->header_for_testing()
                   ->GetSupervisedButtonForTest()
                   ->GetVisible());
}

TEST_F(StatusAreaInternalsHandlerTest, ResetHmrConsentStatus) {
  TestMagicBoostState test_magic_boost_state;

  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  ASSERT_TRUE(magic_boost_state);

  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);
  ASSERT_EQ(chromeos::HMRConsentStatus::kApproved,
            magic_boost_state->hmr_consent_status());

  // `ResetHmrConsentStatus()` should reset the consent status appropriately.
  handler_remote()->ResetHmrConsentStatus();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(chromeos::HMRConsentStatus::kUnset,
            magic_boost_state->hmr_consent_status());
}

class StatusAreaInternalsHandlerBatteryTest
    : public StatusAreaInternalsHandlerTest {
 public:
  FakePowerStatus* GetFakePowerStatus() {
    return handler_->scoped_fake_power_status_->fake_power_status();
  }
};

TEST_F(StatusAreaInternalsHandlerBatteryTest, XIcon) {
  handler_->SetBatteryIcon(
      StatusAreaInternalsHandler::PageHandler::BatteryIcon::kXIcon);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();

  EXPECT_FALSE(fake_power_status->IsBatteryPresent());
  EXPECT_FALSE(fake_power_status->IsUsbChargerConnected());
  EXPECT_FALSE(fake_power_status->IsLinePowerConnected());
  EXPECT_FALSE(fake_power_status->IsBatterySaverActive());
}

TEST_F(StatusAreaInternalsHandlerBatteryTest, UnreliableIcon) {
  handler_->SetBatteryIcon(
      StatusAreaInternalsHandler::PageHandler::BatteryIcon::kUnreliableIcon);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();

  EXPECT_TRUE(fake_power_status->IsBatteryPresent());
  EXPECT_TRUE(fake_power_status->IsUsbChargerConnected());
  EXPECT_FALSE(fake_power_status->IsLinePowerConnected());
  EXPECT_FALSE(fake_power_status->IsBatterySaverActive());
}

TEST_F(StatusAreaInternalsHandlerBatteryTest, BoltIcon) {
  handler_->SetBatteryIcon(
      StatusAreaInternalsHandler::PageHandler::BatteryIcon::kBoltIcon);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();

  EXPECT_TRUE(fake_power_status->IsBatteryPresent());
  EXPECT_FALSE(fake_power_status->IsUsbChargerConnected());
  EXPECT_TRUE(fake_power_status->IsLinePowerConnected());
  EXPECT_FALSE(fake_power_status->IsBatterySaverActive());
}

TEST_F(StatusAreaInternalsHandlerBatteryTest, BatterySaverPlusIcon) {
  handler_->SetBatteryIcon(StatusAreaInternalsHandler::PageHandler::
                               BatteryIcon::kBatterySaverPlusIcon);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();

  EXPECT_TRUE(fake_power_status->IsBatteryPresent());
  EXPECT_FALSE(fake_power_status->IsUsbChargerConnected());
  EXPECT_FALSE(fake_power_status->IsLinePowerConnected());
  EXPECT_TRUE(fake_power_status->IsBatterySaverActive());
}

TEST_F(StatusAreaInternalsHandlerBatteryTest, Percent) {
  handler_->SetBatteryPercent(75);
  FakePowerStatus* fake_power_status = GetFakePowerStatus();

  EXPECT_EQ(fake_power_status->GetBatteryPercent(), 75);
}
}  // namespace ash
