// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_context_menu_model.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using CommandId = ShelfContextMenuModel::CommandId;

class MockNewWindowDelegate
    : public testing::StrictMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void, OpenPersonalizationHub, (), (override));
};

class ShelfContextMenuModelTest
    : public AshTestBase,
      public ::testing::WithParamInterface<user_manager::UserType> {
 public:
  ShelfContextMenuModelTest() = default;

  ShelfContextMenuModelTest(const ShelfContextMenuModelTest&) = delete;
  ShelfContextMenuModelTest& operator=(const ShelfContextMenuModelTest&) =
      delete;

  ~ShelfContextMenuModelTest() override = default;

  void SetUp() override {
    delegate_provider_ = std::make_unique<TestNewWindowDelegateProvider>(
        std::make_unique<MockNewWindowDelegate>());
    AshTestBase::SetUp();
    TestSessionControllerClient* session = GetSessionControllerClient();
    session->AddUserSession("user1@test.com", GetUserType());
    session->SetSessionState(session_manager::SessionState::ACTIVE);
    session->SwitchActiveUser(AccountId::FromUserEmail("user1@test.com"));
  }

  user_manager::UserType GetUserType() const { return GetParam(); }

  MockNewWindowDelegate* GetMockNewWindowDelegate() {
    return static_cast<MockNewWindowDelegate*>(
        delegate_provider_->GetPrimary());
  }

 private:
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

// A test shelf item delegate that records the commands sent for execution.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  TestShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}

  TestShelfItemDelegate(const TestShelfItemDelegate&) = delete;
  TestShelfItemDelegate& operator=(const TestShelfItemDelegate&) = delete;

  ~TestShelfItemDelegate() override = default;

  int last_executed_command() const { return last_executed_command_; }

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {}
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {
    ASSERT_TRUE(from_context_menu);
    last_executed_command_ = command_id;
  }
  void Close() override {}

 private:
  int last_executed_command_ = 0;
};

INSTANTIATE_TEST_SUITE_P(,
                         ShelfContextMenuModelTest,
                         ::testing::Values(user_manager::USER_TYPE_REGULAR,
                                           user_manager::USER_TYPE_CHILD));

// Tests the default items in a shelf context menu.
TEST_P(ShelfContextMenuModelTest, Basic) {
  ShelfContextMenuModel menu(nullptr, GetPrimaryDisplay().id());

  ASSERT_EQ(3u, menu.GetItemCount());
  EXPECT_EQ(CommandId::MENU_AUTO_HIDE, menu.GetCommandIdAt(0));
  EXPECT_EQ(CommandId::MENU_ALIGNMENT_MENU, menu.GetCommandIdAt(1));
  EXPECT_EQ(CommandId::MENU_PERSONALIZATION_HUB, menu.GetCommandIdAt(2));
  for (size_t i = 0; i < menu.GetItemCount(); ++i) {
    EXPECT_TRUE(menu.IsEnabledAt(i));
    EXPECT_TRUE(menu.IsVisibleAt(i));
  }

  // Check the alignment submenu.
  EXPECT_EQ(ui::MenuModel::TYPE_SUBMENU, menu.GetTypeAt(1));
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(1);
  ASSERT_TRUE(submenu);
  ASSERT_EQ(3u, submenu->GetItemCount());
  EXPECT_EQ(CommandId::MENU_ALIGNMENT_LEFT, submenu->GetCommandIdAt(0));
  EXPECT_EQ(CommandId::MENU_ALIGNMENT_BOTTOM, submenu->GetCommandIdAt(1));
  EXPECT_EQ(CommandId::MENU_ALIGNMENT_RIGHT, submenu->GetCommandIdAt(2));
}

// Test invocation of the default menu items.
TEST_P(ShelfContextMenuModelTest, Invocation) {
  int64_t primary_id = GetPrimaryDisplay().id();
  Shelf* shelf = GetPrimaryShelf();

  // Check the shelf auto-hide behavior and menu interaction.
  ShelfContextMenuModel menu1(nullptr, primary_id);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  menu1.ActivatedAt(0);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Recreate the menu, auto-hide should still be enabled.
  ShelfContextMenuModel menu2(nullptr, primary_id);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // By default the shelf should be on bottom, shelf alignment options in order:
  // Left, Bottom, Right. Bottom should be checked.
  ui::MenuModel* submenu = menu2.GetSubmenuModelAt(1);
  EXPECT_TRUE(submenu->IsItemCheckedAt(1));
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  // Activate the left shelf alignment option.
  submenu->ActivatedAt(0);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());

  // Recreate the menu, it should show left alignment checked.
  ShelfContextMenuModel menu3(nullptr, primary_id);
  submenu = menu3.GetSubmenuModelAt(1);
  EXPECT_TRUE(submenu->IsItemCheckedAt(0));
}

TEST_P(ShelfContextMenuModelTest, OpensPersonalizationHubOrWallpaper) {
  int64_t display_id = GetPrimaryDisplay().id();

  ShelfContextMenuModel menu(nullptr, display_id);

  EXPECT_CALL(*GetMockNewWindowDelegate(), OpenPersonalizationHub).Times(1);
  menu.ActivatedAt(2);
}

// Tests custom items in a shelf context menu for an application.
TEST_P(ShelfContextMenuModelTest, CustomItems) {
  // Pass a valid delegate to indicate the menu is for an application.
  TestShelfItemDelegate delegate;
  ShelfContextMenuModel menu(&delegate, GetPrimaryDisplay().id());

  // Because the delegate is valid, the context menu will not have the desktop
  // menu options (autohide, shelf position, and wallpaper picker).
  ASSERT_EQ(0u, menu.GetItemCount());

  // Add some custom items.
  menu.AddItem(203, u"item");
  menu.AddCheckItem(107, u"check");
  menu.AddRadioItem(101, u"radio", 0);
  ui::SimpleMenuModel submenu(nullptr);
  menu.AddSubMenu(55, u"submenu", &submenu);

  // Ensure the menu contents match the items above.
  ASSERT_EQ(4u, menu.GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu.GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu.GetTypeAt(1));
  EXPECT_EQ(ui::MenuModel::TYPE_RADIO, menu.GetTypeAt(2));
  EXPECT_EQ(ui::MenuModel::TYPE_SUBMENU, menu.GetTypeAt(3));

  // Invoking a custom item should execute the command id on the delegate.
  menu.ActivatedAt(1);
  EXPECT_EQ(107, delegate.last_executed_command());
}

// Tests fullscreen's per-display removal of "Autohide shelf": crbug.com/496681
TEST_P(ShelfContextMenuModelTest, AutohideShelfOptionOnExternalDisplay) {
  UpdateDisplay("940x550,940x550");
  int64_t primary_id = GetPrimaryDisplay().id();
  int64_t secondary_id = GetSecondaryDisplay().id();

  // Create a normal window on the primary display.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  widget->SetFullscreen(true);

  ShelfContextMenuModel primary_menu(nullptr, primary_id);
  ShelfContextMenuModel secondary_menu(nullptr, secondary_id);
  EXPECT_FALSE(
      primary_menu.GetIndexOfCommandId(CommandId::MENU_AUTO_HIDE).has_value());
  EXPECT_TRUE(secondary_menu.GetIndexOfCommandId(CommandId::MENU_AUTO_HIDE)
                  .has_value());
}

// Tests that the autohide and alignment menu options are not included in tablet
// mode.
TEST_P(ShelfContextMenuModelTest, ExcludeClamshellOptionsOnTabletMode) {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  int64_t primary_id = GetPrimaryDisplay().id();

  // In tablet mode, the wallpaper picker and auto-hide should be the only two
  // options because other options are disabled.
  tablet_mode_controller->SetEnabledForTest(true);
  ShelfContextMenuModel menu1(nullptr, primary_id);
  EXPECT_EQ(2u, menu1.GetItemCount());
  EXPECT_EQ(ShelfContextMenuModel::MENU_AUTO_HIDE, menu1.GetCommandIdAt(0));
  EXPECT_EQ(ShelfContextMenuModel::MENU_PERSONALIZATION_HUB,
            menu1.GetCommandIdAt(1));

  // Test that a menu shown out of tablet mode includes all three options:
  // MENU_AUTO_HIDE, MENU_ALIGNMENT_MENU.
  tablet_mode_controller->SetEnabledForTest(false);
  ShelfContextMenuModel menu2(nullptr, primary_id);
  EXPECT_EQ(3u, menu2.GetItemCount());

  // Test the auto hide option.
  EXPECT_EQ(ShelfContextMenuModel::MENU_AUTO_HIDE, menu2.GetCommandIdAt(0));
  EXPECT_TRUE(menu2.IsEnabledAt(0));

  // Test the shelf alignment menu option.
  EXPECT_EQ(ShelfContextMenuModel::MENU_ALIGNMENT_MENU,
            menu2.GetCommandIdAt(1));
  EXPECT_TRUE(menu2.IsEnabledAt(1));

  // Test the shelf alignment submenu.
  ui::MenuModel* submenu = menu2.GetSubmenuModelAt(1);
  EXPECT_EQ(ShelfContextMenuModel::MENU_ALIGNMENT_LEFT,
            submenu->GetCommandIdAt(0));
  EXPECT_TRUE(submenu->IsEnabledAt(0));

  EXPECT_EQ(ShelfContextMenuModel::MENU_ALIGNMENT_BOTTOM,
            submenu->GetCommandIdAt(1));
  EXPECT_TRUE(submenu->IsEnabledAt(1));

  EXPECT_EQ(ShelfContextMenuModel::MENU_ALIGNMENT_RIGHT,
            submenu->GetCommandIdAt(2));
  EXPECT_TRUE(submenu->IsEnabledAt(2));

  // Test the personalization hub option.
  EXPECT_EQ(ShelfContextMenuModel::MENU_PERSONALIZATION_HUB,
            menu2.GetCommandIdAt(2));
  EXPECT_TRUE(menu2.IsEnabledAt(2));
}

TEST_P(ShelfContextMenuModelTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(500, ShelfContextMenuModel::MENU_AUTO_HIDE);
  EXPECT_EQ(501, ShelfContextMenuModel::MENU_ALIGNMENT_MENU);
  EXPECT_EQ(502, ShelfContextMenuModel::MENU_ALIGNMENT_LEFT);
  EXPECT_EQ(503, ShelfContextMenuModel::MENU_ALIGNMENT_RIGHT);
  EXPECT_EQ(504, ShelfContextMenuModel::MENU_ALIGNMENT_BOTTOM);
  EXPECT_EQ(506, ShelfContextMenuModel::MENU_PERSONALIZATION_HUB);
}

TEST_P(ShelfContextMenuModelTest, ShelfContextMenuOptions) {
  // Tests that there are exactly 3 shelf context menu options. If you're adding
  // a context menu option ensure that you have added the enum to
  // tools/metrics/enums.xml and that you haven't modified the order of the
  // existing enums.
  ShelfContextMenuModel menu(nullptr, GetPrimaryDisplay().id());
  EXPECT_EQ(3u, menu.GetItemCount());
}

TEST_P(ShelfContextMenuModelTest, NotificationContainerEnabled) {
  // Tests that NOTIFICATION_CONTAINER is enabled. This ensures that the
  // container is able to handle gesture events.
  ShelfContextMenuModel menu(nullptr, GetPrimaryDisplay().id());
  menu.AddItem(NOTIFICATION_CONTAINER, std::u16string());

  EXPECT_TRUE(menu.IsCommandIdEnabled(NOTIFICATION_CONTAINER));
}

}  // namespace
}  // namespace ash
