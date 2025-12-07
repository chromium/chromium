// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/test_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/templates/saved_desk_test_helper.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/birch/coral_chip_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/app_constants/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

namespace ash {

class CoralControllerTest : public AshTestBase {
 public:
  CoralControllerTest() = default;

  void ClickFirstCoralButton() {
    CoralChipButton* coral_button = GetFirstCoralButton();
    CHECK(coral_button);
    LeftClickOn(coral_button);
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
  CoralChipButton* coral_button = GetFirstCoralButton();
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

// Tests that clicking on a in-session chip will stay in Overview and remove the
// chip.
TEST_F(CoralControllerTest, RemoveInSessionChipAfterClicking) {
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  // We are on the only desk before launching the group.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 1);
  ASSERT_EQ(desks_controller->GetActiveDeskIndex(), 0);

  ClickFirstCoralButton();

  // Check that we still in Overview.
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Check that we are on the newly created desk now.
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 2);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);
  ASSERT_EQ(desks_controller->active_desk()->type(), Desk::Type::kCoral);

  // Check that the chip is removed.
  EXPECT_EQ(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips().size(),
      0u);
}

// Tests that visible on all desk window overview items are parented to the
// active desk container once a coral group is launched. Regression test for
// crbug.com/383892354.
TEST_F(CoralControllerTest, VisibleOnAllDeskWindows) {
  // Create two apps with the same app id's as the test coral group. Set one to
  // be visible on all desks.
  auto window1 = CreateAppWindow();
  window1->SetProperty(kAppIDKey,
                       std::string("odknhmnlageboeamepcngndbggdpaobj"));
  window1->SetProperty(aura::client::kWindowWorkspaceKey,
                       aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  auto window2 = CreateAppWindow();
  window2->SetProperty(kAppIDKey,
                       std::string("fkiggjmkendpmbegkagpmagjepfkpmeb"));
  CreateTestGroup({{"Settings", "odknhmnlageboeamepcngndbggdpaobj"},
                   {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
                  "Coral Group");

  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  ClickFirstCoralButton();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ASSERT_EQ(DesksController::Get()->GetNumberOfDesks(), 2);

  // Both items should be parented to the active desk container.
  OverviewItemBase* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItemBase* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_TRUE(desks_util::IsActiveDeskContainer(
      item1->item_widget()->GetNativeWindow()->parent()));
  EXPECT_TRUE(desks_util::IsActiveDeskContainer(
      item2->item_widget()->GetNativeWindow()->parent()));
}

// Tests that there is no crash when removing a coral chip by user.
TEST_F(CoralControllerTest, NoCrashOnRemovingChipByUser) {
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  // Remove the first coral chip and there should be no crash.
  CoralChipButton* coral_chip = GetFirstCoralButton();
  BirchBarController::Get()->OnItemHiddenByUser(coral_chip->GetItem());
}

// Tests that the grouping request contains the initial tab and app entities
// restored on the desk.
TEST_F(CoralControllerTest, RestoreSuppressionContext) {
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")}},
                      "Coral desk"));
  OverrideTestResponse(std::move(test_groups), CoralSource::kPostLogin);

  // Enter overview and click on the chip to restore the items to the active
  // desk.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();

  // Re-enter Overview, the request should contains the restored items.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  // Manually send an in-session request.
  BirchCoralProvider::Get()->HandleInSessionDataRequest();

  const auto& request = BirchCoralProvider::Get()->GetCoralRequestForTest();
  EXPECT_EQ(request.suppression_context().size(), 2u);
  EXPECT_EQ(request.suppression_context()[0]->get_tab()->url,
            GURL("https://google.com/"));
  EXPECT_EQ(request.suppression_context()[1]->get_tab()->url,
            GURL("https://youtube.com/"));
}

// Tests that the grouping request contains the initial tab and app entities
// used to create the desk.
TEST_F(CoralControllerTest, InSessionSuppressionContext) {
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")}},
                      "Coral desk"));
  OverrideTestResponse(std::move(test_groups), CoralSource::kInSession);

  // Enter overview and click on the chip to restore the items to the active
  // desk.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  ClickFirstCoralButton();

  // End and re-enter Overview, the request should contains the restored items.
  Shell::Get()->overview_controller()->EndOverview(OverviewEndAction::kTests);
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  // Manually send an in-session request.
  BirchCoralProvider::Get()->HandleInSessionDataRequest();

  const auto& request = BirchCoralProvider::Get()->GetCoralRequestForTest();
  EXPECT_EQ(request.suppression_context().size(), 2u);
  EXPECT_EQ(request.suppression_context()[0]->get_tab()->url,
            GURL("https://google.com/"));
  EXPECT_EQ(request.suppression_context()[1]->get_tab()->url,
            GURL("https://youtube.com/"));
}

class CoralSavedGroupTest : public CoralControllerTest {
 public:
  desks_storage::DeskModel* desk_model() {
    return ash_test_helper()->saved_desk_test_helper()->desk_model();
  }

  void AddCoralEntry(const std::string& name) {
    AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(), name,
                      base::Time::Now(), DeskTemplateType::kCoral);
  }

  // Right click on the coral button to bring up the context menu and return the
  // save as group option menu item.
  views::MenuItemView* GetSaveAsGroupMenuItem() {
    RightClickOn(GetFirstCoralButton());
    BirchBarMenuModelAdapter* model_adapter =
        BirchBarController::Get()->chip_menu_model_adapter_for_testing();
    EXPECT_TRUE(model_adapter->IsShowingMenu());
    views::MenuItemView* save_as_group_item =
        model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(1);
    if (!save_as_group_item ||
        save_as_group_item->GetCommand() !=
            base::to_underlying(
                BirchChipContextMenuModel::CommandId::kCoralSaveForLater)) {
      return nullptr;
    }
    return save_as_group_item;
  }

  void EnterOverviewAndSaveGroupAsTemplate() {
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kTests);
    views::MenuItemView* save_as_group_item = GetSaveAsGroupMenuItem();
    LeftClickOn(save_as_group_item);
  }

  void SetUp() override {
    CoralControllerTest::SetUp();
    ash_test_helper()->saved_desk_test_helper()->WaitForDeskModels();
  }
};

// Tests that the saved as group menu item does not show up in tablet mode.
TEST_F(CoralSavedGroupTest, NoMenuInTablet) {
  TabletModeControllerTestApi().EnterTabletMode();
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  EXPECT_FALSE(GetSaveAsGroupMenuItem());
}

// Tests that the saved as group menu item does not show up in an informed
// restore overview session.
TEST_F(CoralSavedGroupTest, NoMenuInInformedRestore) {
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests, OverviewEnterExitType::kInformedRestore);
  EXPECT_FALSE(GetSaveAsGroupMenuItem());
}

// Tests saving a group that has a couple tabs in it.
TEST_F(CoralSavedGroupTest, SaveBrowserInGroup) {
  // Prepare a coral response with two tabs.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")}},
                      "Coral desk"));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Verify the desk model entry name and type.
  const desks_storage::DeskModel::GetAllEntriesResult& result =
      desk_model()->GetAllEntries();
  ASSERT_EQ(result.entries.size(), 1u);
  const DeskTemplate* coral_template = result.entries[0];
  EXPECT_EQ(coral_template->template_name(), u"Coral desk");
  EXPECT_EQ(coral_template->type(), DeskTemplateType::kCoral);

  // Verify that the desk model entry browser info matches our fake coral
  // response.
  const app_restore::RestoreData* restore_data =
      coral_template->desk_restore_data();
  ASSERT_TRUE(restore_data);
  const app_restore::AppRestoreData* app_restore_data =
      restore_data->GetAppRestoreData(app_constants::kChromeAppId,
                                      /*window_id=*/0);
  ASSERT_TRUE(app_restore_data);
  app_restore::BrowserExtraInfo browser_extra_info =
      app_restore_data->browser_extra_info;
  EXPECT_THAT(browser_extra_info.urls,
              testing::ElementsAre(GURL("https://google.com/"),
                                   GURL("https://youtube.com/")));
}

// Tests saving a group with an empty (invalid) title.
TEST_F(CoralSavedGroupTest, SaveEmptyTitleGroup) {
  // Prepare a coral group with an empty title.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")}},
                      ""));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Verify the desk model entry name and type.
  const desks_storage::DeskModel::GetAllEntriesResult& result =
      desk_model()->GetAllEntries();
  ASSERT_EQ(result.entries.size(), 1u);
  const DeskTemplate* coral_template = result.entries[0];
  EXPECT_EQ(coral_template->template_name(),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CORAL_SUGGESTION_NAME));
  EXPECT_EQ(coral_template->type(), DeskTemplateType::kCoral);
}

// Tests saving a group with title in generation.
TEST_F(CoralSavedGroupTest, SaveNullTitleGroup) {
  // Prepare a null titled group.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")}},
                      std::nullopt));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Verify the desk model entry name and type.
  const desks_storage::DeskModel::GetAllEntriesResult& result =
      desk_model()->GetAllEntries();
  ASSERT_EQ(result.entries.size(), 1u);
  const DeskTemplate* coral_template = result.entries[0];
  EXPECT_EQ(coral_template->template_name(),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CORAL_SUGGESTION_NAME));
  EXPECT_EQ(coral_template->type(), DeskTemplateType::kCoral);
}

// Tests saving a group that has a couple apps in it.
TEST_F(CoralSavedGroupTest, SaveAppsInGroup) {
  // Create some windows with app ids.
  auto window1 = CreateAppWindow();
  auto window2 = CreateAppWindow();
  auto window3 = CreateAppWindow();
  window1->SetProperty(kAppIDKey, std::string("window1_app_id"));
  window2->SetProperty(kAppIDKey, std::string("window2_app_id"));
  window3->SetProperty(kAppIDKey, std::string("window3_app_id"));

  // Simulate having app launch info for these windows.
  static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate())
      ->set_app_ids_with_app_launch_info(
          {"window1_app_id", "window2_app_id", "window3_app_id"});

  // Prepare a coral response with two of the windows.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(CreateTestGroup(
      {{"Window1", "window1_app_id"}, {"Window2", "window2_app_id"}},
      "Coral desk"));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Verify the desk model entry name and type.
  const desks_storage::DeskModel::GetAllEntriesResult& result =
      desk_model()->GetAllEntries();
  ASSERT_EQ(result.entries.size(), 1u);
  const DeskTemplate* coral_template = result.entries[0];
  EXPECT_EQ(coral_template->template_name(), u"Coral desk");
  EXPECT_EQ(coral_template->type(), DeskTemplateType::kCoral);

  // Verify that the desk model entry browser info matches our fake coral
  // response.
  const app_restore::RestoreData* restore_data =
      coral_template->desk_restore_data();
  ASSERT_TRUE(restore_data);
  const app_restore::RestoreData::AppIdToLaunchList& launch_list =
      restore_data->app_id_to_launch_list();
  EXPECT_TRUE(launch_list.contains("window1_app_id"));
  EXPECT_TRUE(launch_list.contains("window2_app_id"));
  EXPECT_FALSE(launch_list.contains("window3_app_id"));
}

// Tests that after saving a group, we show the saved desk library.
TEST_F(CoralSavedGroupTest, ShowSavedDeskLibrary) {
  // Prepare a coral response.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")}}, "Coral desk"));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Tests that the saved desk library is shown.
  EXPECT_TRUE(base::test::RunUntil([]() {
    OverviewSession* session = OverviewController::Get()->overview_session();
    return session && session->IsShowingSavedDeskLibrary();
  }));
}

TEST_F(CoralSavedGroupTest, MaxCoralSavedGroupLimit) {
  // Add enough entries to hit the max.
  for (int i = 0; i < 6; ++i) {
    AddCoralEntry(base::NumberToString(i));
  }

  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  views::MenuItemView* save_as_group_item = GetSaveAsGroupMenuItem();

  // Left click on the menu item and test that the toast shows up since we
  // cannot create more coral saved groups.
  LeftClickOn(save_as_group_item);
  EXPECT_TRUE(Shell::Get()->toast_manager()->IsToastShown(
      "coral_max_saved_groups_toast"));
}

// Tests that clicking a saved group item creates a coral desk.
TEST_F(CoralSavedGroupTest, LaunchSavedGroup) {
  AddCoralEntry("saved_group_1");

  // Ensure we have one desk currently.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 1);

  // Enter overview, ensure the library button is visible and click it.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  RunScheduledLayoutForAllOverviewDeskBars();
  auto* button = GetLibraryButton();
  ASSERT_TRUE(button);
  ASSERT_TRUE(button->GetVisible());
  LeftClickOn(button);

  // Click on the only saved desk entry.
  const views::Button* saved_group_launch_button =
      GetSavedDeskItemButton(/*index=*/0);
  LeftClickOn(saved_group_launch_button);

  // Test that we create a new desk of type coral.
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 2);
  EXPECT_EQ(desks_controller->desks().back()->type(), Desk::Type::kCoral);
}

// Tests that the saved desk library has the expected amount of grid items.
TEST_F(CoralSavedGroupTest, CheckGridItems) {
  AddCoralEntry("saved_group_1");
  AddCoralEntry("saved_group_2");

  // Enter overview, ensure the library button is visible and click it.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  RunScheduledLayoutForAllOverviewDeskBars();
  auto* button = GetLibraryButton();
  ASSERT_TRUE(button);
  ASSERT_TRUE(button->GetVisible());
  LeftClickOn(button);

  // Test that the library view has the coral grid with two coral entries.
  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  SavedDeskLibraryView* library_view = overview_grid->GetSavedDeskLibraryView();
  const SavedDeskGridView* coral_grid_view =
      SavedDeskLibraryViewTestApi(library_view).coral_grid_view();
  EXPECT_EQ(coral_grid_view->grid_items().size(), 2u);
}

// Tests that the suppression context will be saved in the desk template.
TEST_F(CoralSavedGroupTest, SaveSuppressionContext) {
  // Create some windows with app ids.
  auto window1 = CreateAppWindow();
  auto window2 = CreateAppWindow();
  window1->SetProperty(kAppIDKey, std::string("window1_app_id"));
  window2->SetProperty(kAppIDKey, std::string("window2_app_id"));

  // Simulate having app launch info for these windows.
  static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate())
      ->set_app_ids_with_app_launch_info({"window1_app_id", "window2_app_id"});

  // Prepare a coral response.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com/")},
                       {"Youtube", GURL("https://youtube.com/")},
                       {"Window1", "window1_app_id"},
                       {"Window2", "window2_app_id"}},
                      "Coral desk"));
  OverrideTestResponse(std::move(test_groups));

  EnterOverviewAndSaveGroupAsTemplate();

  // Tests that the saved desk library is shown.
  EXPECT_TRUE(base::test::RunUntil([]() {
    OverviewSession* session = OverviewController::Get()->overview_session();
    return session && session->IsShowingSavedDeskLibrary();
  }));

  // Click on the only saved desk entry.
  const views::Button* saved_group_launch_button =
      GetSavedDeskItemButton(/*index=*/0);
  LeftClickOn(saved_group_launch_button);

  // We create a new desk of type coral.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 2);
  ASSERT_EQ(desks_controller->desks().back()->type(), Desk::Type::kCoral);

  // End and activate the coral desk.
  Shell::Get()->overview_controller()->EndOverview(OverviewEndAction::kTests);
  ActivateDesk(desks_controller->desks().back().get());

  // Re-enter Overview, the request should contains the restored items.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  // Manually send an in-session request.
  BirchCoralProvider::Get()->HandleInSessionDataRequest();

  const auto& request = BirchCoralProvider::Get()->GetCoralRequestForTest();
  ASSERT_EQ(request.suppression_context().size(), 4u);
  EXPECT_EQ(request.suppression_context()[0]->get_tab()->url,
            GURL("https://google.com/"));
  EXPECT_EQ(request.suppression_context()[1]->get_tab()->url,
            GURL("https://youtube.com/"));
  EXPECT_EQ(request.suppression_context()[2]->get_app()->id, "window1_app_id");
  EXPECT_EQ(request.suppression_context()[3]->get_app()->id, "window2_app_id");
}

}  // namespace ash
