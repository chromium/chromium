// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_task_test_utils.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/user_type.h"
#include "ui/views/view_utils.h"

namespace ash {

class FocusModeFeaturePodControllerTest : public AshTestBase {
 public:
  FocusModeFeaturePodControllerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFocusMode},
        /*disabled_features=*/{});
  }
  ~FocusModeFeaturePodControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // In order to create a detailed view, we need to check the network.
    // `g_network_handler` is null in tests, so we need to manually set the
    // network connected state.
    glanceables_util::SetIsNetworkConnectedForTest(true);

    // Focus Mode considers it to be a first time user flow if
    // `kFocusModeDoNotDisturb` has never been set by the user before. For
    // normal feature testing purposes, we will intentionally set it so that the
    // pref will not be marked as using the default value.
    prefs()->SetBoolean(prefs::kFocusModeDoNotDisturb, true);

    auto& tasks_client =
        CreateFakeTasksClient(AccountId::FromUserEmail("user0@tray"));
    AddFakeTaskList(tasks_client, "default");
    AddFakeTask(tasks_client, "default", "task1", "Task 1");

    CreateFakeFocusModeTile();
  }

  void TearDown() override {
    controller_.reset();
    tile_.reset();
    AshTestBase::TearDown();
  }

  void CreateFakeFocusModeTile() {
    // We need to show the bubble in order to get the
    // `unified_system_tray_controller`.
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    controller_ = std::make_unique<FocusModeFeaturePodController>(
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller());
    tile_ = controller_->CreateTile();
  }

  void ExpectFocusModeDetailedViewShown() {
    TrayDetailedView* detailed_view =
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->quick_settings_view()
            ->GetDetailedViewForTest<TrayDetailedView>();
    ASSERT_TRUE(detailed_view);
    EXPECT_TRUE(views::IsViewClass<FocusModeDetailedView>(detailed_view));
  }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FocusModeFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

// Tests that the tile is normally visible, and is not visible when the screen
// is locked.
TEST_F(FocusModeFeaturePodControllerTest, TileVisibility) {
  // The tile is visible in an active session.
  EXPECT_TRUE(tile_->GetVisible());

  // Locking the screen closes the system tray bubble, so we need to create the
  // tile again.
  GetSessionControllerClient()->LockScreen();
  CreateFakeFocusModeTile();

  // Verify that the tile should not be visible at the lock screen.
  EXPECT_FALSE(tile_->GetVisible());
}

// Tests tile visibility for all different user types. This was added for
// b/344040908 to prevent crashes when ephemeral accounts try to use Focus Mode.
TEST_F(FocusModeFeaturePodControllerTest, TileVisibilityForUserTypes) {
  struct {
    std::string trace;
    user_manager::UserType user_type;
    bool is_tile_visible;
  } kUserTypeTestCases[] = {
      {"regular user", user_manager::UserType::kRegular, true},
      {"child", user_manager::UserType::kChild, true},
      {"guest", user_manager::UserType::kGuest, false},
      {"public account", user_manager::UserType::kPublicAccount, false},
      {"kiosk app", user_manager::UserType::kKioskApp, false},
      {"web kiosk app", user_manager::UserType::kWebKioskApp, false},
  };

  for (const auto& test_case : kUserTypeTestCases) {
    SCOPED_TRACE(test_case.trace);
    ClearLogin();
    SimulateUserLogin("example@gmail.com", test_case.user_type);

    CreateFakeFocusModeTile();
    EXPECT_EQ(test_case.is_tile_visible, tile_->GetVisible());
  }
}

// Tests that pressing the icon works and toggles a Focus Mode Session.
TEST_F(FocusModeFeaturePodControllerTest, PressIconTogglesFocusModeSession) {
  base::HistogramTester histogram_tester;
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(tile_->GetVisible());
  EXPECT_TRUE(tile_->GetEnabled());

  // Verify that clicking the icon and starting the Focus Mode Session closes
  // the bubble.
  controller_->OnIconPressed();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Recreate the tile since the bubble was closed.
  CreateFakeFocusModeTile();
  EXPECT_TRUE(tile_->IsToggled());

  // End the focus session. This should not close the bubble.
  controller_->OnIconPressed();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(tile_->IsToggled());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::
          kToggleEndButtonDuringSessionHistogramName,
      /*sample=*/focus_mode_histogram_names::ToggleSource::kFeaturePod,
      /*expected_count=*/1);
}

// Verify that when toggling a focus mode through the QS tile or the focus
// panel, the histogram will record it.
TEST_F(FocusModeFeaturePodControllerTest, CheckStartSessionSourceHistograms) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // 1. Start a focus session from the feature pod.
  controller->ToggleFocusMode(
      focus_mode_histogram_names::ToggleSource::kFeaturePod);
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kStartSessionSourceHistogramName,
      /*sample=*/focus_mode_histogram_names::StartSessionSource::kFeaturePod,
      /*expected_count=*/1);

  controller->ToggleFocusMode(
      focus_mode_histogram_names::ToggleSource::kFeaturePod);
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a new focus session from the focus panel.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kStartSessionSourceHistogramName,
      /*sample=*/focus_mode_histogram_names::StartSessionSource::kFocusPanel,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kStartSessionSourceHistogramName, 2);
}

// Tests that pressing the label works and shows the `FocusModeDetailedView`.
TEST_F(FocusModeFeaturePodControllerTest, PressLabelEntersFocusPanel) {
  controller_->OnLabelPressed();
  ExpectFocusModeDetailedViewShown();
}

// Verify that the tile operates correctly for the first time user flow. This
// includes:
// - The session duration is hidden.
// - Clicking the tile icon shows to the focus mode detailed view.
TEST_F(FocusModeFeaturePodControllerTest, FirstTimeUserFlow) {
  // Clear `kFocusModeDoNotDisturb` to trigger the first time user flow.
  prefs()->ClearPref(prefs::kFocusModeDoNotDisturb);

  // Recreate the tile so that the UI is updated after we set the user pref.
  CreateFakeFocusModeTile();

  // Verify that the tile sub label is hidden for the first time user flow.
  auto* focus_mode_controller = FocusModeController::Get();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(tile_->sub_label()->GetVisible());

  // Verify that clicking the icon does not start the focus session for the
  // first time user flow, but instead shows the focus mode detailed view.
  controller_->OnIconPressed();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  ExpectFocusModeDetailedViewShown();

  // Start a session, and recreate the tile since the bubble was closed.
  focus_mode_controller->ToggleFocusMode();
  CreateFakeFocusModeTile();

  // Verify that the tile sub label should be visible.
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(tile_->sub_label()->GetVisible());

  // End a session. Check that the tile session duration text is now visible,
  // and is not hidden since we are no longer in the first time user flow.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(tile_->sub_label()->GetVisible());

  // Verify that clicking the icon goes back to the normal flow of and starting
  // a Focus Mode Session.
  controller_->OnIconPressed();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
}

}  // namespace ash
