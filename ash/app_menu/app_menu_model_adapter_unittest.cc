// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/app_menu_model_adapter.h"

#include <memory>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/menu_test_utils.h"

namespace ash {
namespace {

class TestAppMenuModelAdapter : public AppMenuModelAdapter {
 public:
  explicit TestAppMenuModelAdapter(std::unique_ptr<ui::SimpleMenuModel> model)
      : AppMenuModelAdapter("test-app-id",
                            std::move(model),
                            nullptr,
                            ui::MENU_SOURCE_MOUSE,
                            base::OnceClosure(),
                            false) {}
  TestAppMenuModelAdapter(const TestAppMenuModelAdapter&) = delete;
  TestAppMenuModelAdapter& operator=(const TestAppMenuModelAdapter&) = delete;
  ~TestAppMenuModelAdapter() override = default;

  int last_executed_command_id() { return last_executed_command_id_; }

 private:
  void ExecuteCommand(int id, int mouse_event_flags) override {
    AppMenuModelAdapter::ExecuteCommand(id, mouse_event_flags);
    last_executed_command_id_ = id;
  }

  void RecordHistogramOnMenuClosed() override {}

  int last_executed_command_id_;
};

}  // namespace

class AppMenuModelAdapterTest : public AshTestBase {
 public:
  AppMenuModelAdapterTest() = default;
  AppMenuModelAdapterTest(const AppMenuModelAdapterTest&) = delete;
  AppMenuModelAdapterTest& operator=(const AppMenuModelAdapterTest&) = delete;
  ~AppMenuModelAdapterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    views::test::DisableMenuClosureAnimations();

    auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    submenu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model->AddItem(MENU_CLOSE, u"item 0");
    submenu_model_->AddItem(USE_LAUNCH_TYPE_REGULAR, u"item 2");
    menu_model->AddActionableSubMenu(LAUNCH_NEW, u"item 1",
                                     submenu_model_.get());
    app_menu_model_adapter_ =
        std::make_unique<TestAppMenuModelAdapter>(std::move(menu_model));
  }

  void ClickOnMenuItemForCommandId(int command_id) {
    auto* target_menu_item =
        app_menu_model_adapter_->root_for_testing()->GetMenuItemByID(
            command_id);
    auto location = target_menu_item->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(location);
    event_generator->ClickLeftButton();
  }

  bool IsShowingSubmenuForCommandId(int command_id) {
    return app_menu_model_adapter()
        ->root_for_testing()
        ->GetMenuItemByID(command_id)
        ->SubmenuIsShowing();
  }

  void OpenMenu() {
    app_menu_model_adapter_->Run(gfx::Rect(),
                                 views::MenuAnchorPosition::kBottomCenter,
                                 views::MenuRunner::CONTEXT_MENU |
                                     views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                                     views::MenuRunner::FIXED_ANCHOR);
  }

  void ShowSubmenuForCommandId(int command_id) {
    auto* menu_item_view = app_menu_model_adapter_->root_for_testing();
    menu_item_view->GetMenuController()->SelectItemAndOpenSubmenu(
        menu_item_view->GetMenuItemByID(command_id));
  }

  TestAppMenuModelAdapter* app_menu_model_adapter() {
    return app_menu_model_adapter_.get();
  }

 private:
  std::unique_ptr<TestAppMenuModelAdapter> app_menu_model_adapter_;
  std::unique_ptr<ui::SimpleMenuModel> submenu_model_;
};

TEST_F(AppMenuModelAdapterTest, ShouldCloseMenuOnNonUseLaunchTypeCommand) {
  EXPECT_FALSE(app_menu_model_adapter()->IsShowingMenu());
  OpenMenu();
  EXPECT_TRUE(app_menu_model_adapter()->IsShowingMenu());

  ClickOnMenuItemForCommandId(MENU_CLOSE);
  EXPECT_FALSE(app_menu_model_adapter()->IsShowingMenu());
  EXPECT_EQ(app_menu_model_adapter()->last_executed_command_id(), MENU_CLOSE);
}

TEST_F(AppMenuModelAdapterTest, ShouldCloseMenuOnLaunchNewCommand) {
  EXPECT_FALSE(app_menu_model_adapter()->IsShowingMenu());
  OpenMenu();
  EXPECT_TRUE(app_menu_model_adapter()->IsShowingMenu());

  ClickOnMenuItemForCommandId(LAUNCH_NEW);
  EXPECT_FALSE(app_menu_model_adapter()->IsShowingMenu());
  EXPECT_EQ(app_menu_model_adapter()->last_executed_command_id(), LAUNCH_NEW);
}

TEST_F(AppMenuModelAdapterTest, ShouldKeepMenuOpenOnUseLaunchTypeCommand) {
  EXPECT_FALSE(app_menu_model_adapter()->IsShowingMenu());
  OpenMenu();
  EXPECT_TRUE(app_menu_model_adapter()->IsShowingMenu());

  EXPECT_FALSE(IsShowingSubmenuForCommandId(LAUNCH_NEW));
  ShowSubmenuForCommandId(LAUNCH_NEW);
  EXPECT_TRUE(IsShowingSubmenuForCommandId(LAUNCH_NEW));

  ClickOnMenuItemForCommandId(USE_LAUNCH_TYPE_REGULAR);
  EXPECT_FALSE(IsShowingSubmenuForCommandId(LAUNCH_NEW));
  EXPECT_TRUE(app_menu_model_adapter()->IsShowingMenu());
  EXPECT_EQ(app_menu_model_adapter()->last_executed_command_id(),
            USE_LAUNCH_TYPE_REGULAR);
}

}  // namespace ash
