// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/status_area_internals/status_area_internals_handler.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
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
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

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
    handler_ = std::make_unique<StatusAreaInternalsHandler>(
        handler_remote_.BindNewPipeAndPassReceiver());

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kVideoConference,
                              features::kCameraEffectsSupportedByHardware},
        /*disabled_features=*/{});

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    // Need to use test resources instead to have `AshTestBase` work on
    // //ash/webui.
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();

    AshTestBase::SetUp();
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;

  mojo::Remote<mojom::status_area_internals::PageHandler> handler_remote_;

  std::unique_ptr<StatusAreaInternalsHandler> handler_;
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
      // Projector Annotation Tray
      ToggleTrayTestParam{
          "Projector Annotation Tray",
          GetStatusAreaWidget()->projector_annotation_tray(),
          base::BindRepeating(
              &mojom::status_area_internals::PageHandler::ToggleProjectorTray,
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

}  // namespace ash
