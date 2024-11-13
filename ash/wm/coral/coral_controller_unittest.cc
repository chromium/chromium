// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/test_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/view_utils.h"

namespace ash {

class CoralControllerTest : public AshTestBase {
 public:
  void ClickFirstCoralButton() {
    DeskSwitchAnimationWaiter waiter;
    BirchChipButton* coral_button = GetFirstCoralButton();
    CHECK(coral_button);
    LeftClickOn(coral_button);
    waiter.Wait();
  }

  void SetUp() override {
    AshTestBase::SetUp();

    // Create test birch client.
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());

    base::RunLoop run_loop;
    birch_model->GetItemRemoverForTest()->SetProtoInitCallbackForTest(
        run_loop.QuitClosure());
    run_loop.Run();

    // Prepare a coral response so we have a coral glanceable to click.
    std::vector<coral::mojom::GroupPtr> test_groups;
    test_groups.push_back(CreateDefaultTestGroup());
    OverrideTestResponse(std::move(test_groups));
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    birch_client_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<TestBirchClient> birch_client_;

  base::test::ScopedFeatureList feature_list_{features::kCoralFeature};
};

// Tests that clicking the in session coral button opens and activates a new
// desk.
TEST_F(CoralControllerTest, OpenNewDesk) {
  // Click the coral button and verify we have created and activated the new
  // desk.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();
  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), 2u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);
}

// Tests that clicking the coral chip and then it's addon view will not crash.
// Regression test for crbug.com/376549527.
TEST_F(CoralControllerTest, ClickChipWithMaxDesks) {
  // Add desks until no longer possible.
  while (DesksController::Get()->CanCreateDesks()) {
    NewDesk();
  }

  // Click the coral button and then the addon view when we have max desks.
  // Verify that there is no crash and the expected toast is shown.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  BirchChipButton* coral_button = GetFirstCoralButton();
  LeftClickOn(coral_button);
  LeftClickOn(coral_button->addon_view());
  EXPECT_TRUE(
      Shell::Get()->toast_manager()->IsToastShown("coral_max_desks_toast"));
}

// Tests that there is no crash if we get a async title update after exiting
// overview. Regression test for http://crbug.com/378894754.
TEST_F(CoralControllerTest, NoCrashOnTitleUpdate) {
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  overview_controller->EndOverview(OverviewEndAction::kTests);

  // Simulate an async title update after overview has ended. There should be no
  // crash.
  BirchCoralProvider::Get()->TitleUpdated(base::Token(), std::string());
}

// Tests that a window that launches onto a coral desk maintains its visible on
// all desks property.
TEST_F(CoralControllerTest, VisibleOnAllDesks) {
  auto app_window = CreateAppWindow();
  // This is the property of one of the apps in the group
  // `CreateDefaultTestGroup()`, which is used in the test setup harness.
  app_window->SetProperty(kAppIDKey,
                          std::string("odknhmnlageboeamepcngndbggdpaobj"));
  app_window->SetProperty(aura::client::kWindowWorkspaceKey,
                          aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);

  // Click the coral button and verify we have created and activated the new
  // desk.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();
  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), 2u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);

  // Test that the window is moved onto the new desk and still is visible on all
  // desks.
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(app_window.get()));
  EXPECT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(app_window.get()));
}

// Tests that when we have a snap group with one window in the coral group, only
// the window in the coral group gets moved to the new coral desk.
TEST_F(CoralControllerTest, SnapGroupOneWindowInCoralGroup) {
  auto app_window_in_group = CreateAppWindow();
  // This is the property of one of the apps in the group
  // `CreateDefaultTestGroup()`, which is used in the test setup harness.
  app_window_in_group->SetProperty(
      kAppIDKey, std::string("odknhmnlageboeamepcngndbggdpaobj"));
  auto app_window_not_in_group = CreateAppWindow();

  SnapTwoTestWindows(app_window_in_group.get(), app_window_not_in_group.get(),
                     /*horizontal=*/true, GetEventGenerator());
  ASSERT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(
      app_window_not_in_group.get(), app_window_in_group.get()));

  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();

  // Tests that the two windows are on separate desks, and that there are no
  // snap groups.
  const std::vector<std::unique_ptr<Desk>>& desks =
      DesksController::Get()->desks();
  EXPECT_TRUE(
      base::Contains(desks[0]->windows(), app_window_not_in_group.get()));
  EXPECT_TRUE(base::Contains(desks[1]->windows(), app_window_in_group.get()));
  EXPECT_FALSE(SnapGroupController::Get()->AreWindowsInSnapGroup(
      app_window_not_in_group.get(), app_window_in_group.get()));
}

// Tests that when we have a snap group with both windows in the coral group,
// both windows move to the coral desk, and the snap group is maintained.
TEST_F(CoralControllerTest, SnapGroupTwoWindowsInCoralGroup) {
  // These are the properties of two of the apps in the group
  // `CreateDefaultTestGroup()`, which is used in the test setup harness.
  auto window1 = CreateAppWindow();
  window1->SetProperty(kAppIDKey,
                       std::string("odknhmnlageboeamepcngndbggdpaobj"));
  auto window2 = CreateAppWindow();
  window2->SetProperty(kAppIDKey,
                       std::string("fkiggjmkendpmbegkagpmagjepfkpmeb"));

  SnapTwoTestWindows(window1.get(), window2.get(),
                     /*horizontal=*/true, GetEventGenerator());
  ASSERT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window1.get(),
                                                                window2.get()));

  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();

  // Tests that the two windows are on new desk and still in a snap group.
  const std::vector<std::unique_ptr<Desk>>& desks =
      DesksController::Get()->desks();
  EXPECT_TRUE(base::Contains(desks[1]->windows(), window1.get()));
  EXPECT_TRUE(base::Contains(desks[1]->windows(), window2.get()));
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window1.get(),
                                                                window2.get()));
}

}  // namespace ash
