// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_party_feature_pod_controller.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

class ShelfTest : public AshTestBase {
 public:
  ShelfTest() = default;

  ShelfTest(const ShelfTest&) = delete;
  ShelfTest& operator=(const ShelfTest&) = delete;

  ~ShelfTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();
    shelf_model_ = shelf_view_->model();

    test_ = std::make_unique<ShelfViewTestAPI>(shelf_view_);
  }

  ShelfView* shelf_view() { return shelf_view_; }
  ShelfModel* shelf_model() { return shelf_model_; }

  ShelfViewTestAPI* test_api() { return test_.get(); }

 protected:
  Shelf* GetSecondaryShelf() {
    return Shell::GetRootWindowControllerWithDisplayId(
               GetSecondaryDisplay().id())
        ->shelf();
  }

 private:
  ShelfView* shelf_view_ = nullptr;
  ShelfModel* shelf_model_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> test_;
};

// Confirms that ShelfItem reflects the appropriated state.
TEST_F(ShelfTest, StatusReflection) {
  // Initially we have the app list.
  size_t button_count = test_api()->GetButtonCount();

  // Add a running app.
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(
      item, std::make_unique<TestShelfItemDelegate>(item.id));
  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfAppButton* button = test_api()->GetButton(index);
  EXPECT_EQ(ShelfAppButton::STATE_RUNNING, button->state());

  // Remove it.
  shelf_model()->RemoveItemAt(index);
  ASSERT_EQ(--button_count, test_api()->GetButtonCount());
}

// Confirm that using the menu will clear the hover attribute. To avoid another
// browser test we check this here.
TEST_F(ShelfTest, CheckHoverAfterMenu) {
  // Initially we have the app list.
  size_t button_count = test_api()->GetButtonCount();

  // Add a running app.
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(
      item, std::make_unique<TestShelfItemDelegate>(item.id));

  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfAppButton* button = test_api()->GetButton(index);
  button->AddState(ShelfAppButton::STATE_HOVERED);
  button->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_HOVERED);

  // Remove it.
  shelf_model()->RemoveItemAt(index);
}

// Various assertions around auto-hide behavior.
TEST_F(ShelfTest, ToggleAutoHide) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();
  wm::ActivateWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
}

// Tests if shelf is hidden on secondary display after the primary display is
// changed.
TEST_F(ShelfTest, ShelfHiddenOnScreenOnSecondaryDisplay) {
  for (const auto& state : {session_manager::SessionState::LOCKED,
                            session_manager::SessionState::LOGIN_PRIMARY}) {
    SCOPED_TRACE(base::StringPrintf("Testing state: %d", state));
    GetSessionControllerClient()->SetSessionState(state);
    UpdateDisplay("800x600,800x600");

    EXPECT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
    EXPECT_EQ(SHELF_HIDDEN, GetSecondaryShelf()->GetVisibilityState());

    SwapPrimaryDisplay();

    EXPECT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
    EXPECT_EQ(SHELF_HIDDEN, GetSecondaryShelf()->GetVisibilityState());
  }
}

using NoSessionShelfTest = NoSessionAshTestBase;

// Regression test for crash in Shelf::SetAlignment(). https://crbug.com/937495
TEST_F(NoSessionShelfTest, SetAlignmentDuringDisplayDisconnect) {
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();

  // The task indirectly triggers Shelf::SetAlignment() via a SessionObserver.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TestSessionControllerClient* session) {
            session->SetSessionState(session_manager::SessionState::ACTIVE);
          },
          GetSessionControllerClient()));

  // Remove the secondary display.
  UpdateDisplay("1280x1024");

  // The session activation task runs before the RootWindowController and the
  // Shelf are deleted.
  base::RunLoop().RunUntilIdle();

  // No crash.
}

class ShelfPartyQsTileTest : public NoSessionAshTestBase {
 public:
  ShelfPartyQsTileTest() = default;
  ShelfPartyQsTileTest(const ShelfPartyQsTileTest&) = delete;
  ShelfPartyQsTileTest& operator=(const ShelfPartyQsTileTest&) = delete;
  ~ShelfPartyQsTileTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    shelf_model_ = GetPrimaryShelf()->GetShelfViewForTesting()->model();
    qs_tile_controller_ = std::make_unique<ShelfPartyFeaturePodController>();
    qs_tile_button_view_.reset(qs_tile_controller_->CreateButton());
  }

  void TearDown() override {
    qs_tile_controller_.reset();
    qs_tile_button_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  ShelfModel* shelf_model() { return shelf_model_; }
  ShelfPartyFeaturePodController* qs_tile_controller() {
    return qs_tile_controller_.get();
  }
  FeaturePodButton* qs_tile_button_view() { return qs_tile_button_view_.get(); }

 private:
  ShelfModel* shelf_model_ = nullptr;
  std::unique_ptr<ShelfPartyFeaturePodController> qs_tile_controller_;
  std::unique_ptr<FeaturePodButton> qs_tile_button_view_;
};

TEST_F(ShelfPartyQsTileTest, VisibleWhenUserSessionIsActive) {
  EXPECT_FALSE(qs_tile_button_view()->GetVisible());
  auto* session_controller = GetSessionControllerClient();
  session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(qs_tile_button_view()->GetVisible());
  session_controller->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(qs_tile_button_view()->GetVisible());
}

TEST_F(ShelfPartyQsTileTest, InvisibleWhenEnterpriseManaged) {
  auto* session_controller = GetSessionControllerClient();
  session_controller->set_is_enterprise_managed(true);
  session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(qs_tile_button_view()->GetVisible());
}

TEST_F(ShelfPartyQsTileTest, OnIconPressed) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(shelf_model()->in_shelf_party());
  EXPECT_FALSE(qs_tile_button_view()->IsToggled());
  qs_tile_controller()->OnIconPressed();
  EXPECT_TRUE(shelf_model()->in_shelf_party());
  EXPECT_TRUE(qs_tile_button_view()->IsToggled());
  qs_tile_controller()->OnIconPressed();
  EXPECT_FALSE(shelf_model()->in_shelf_party());
  EXPECT_FALSE(qs_tile_button_view()->IsToggled());
}

TEST_F(ShelfPartyQsTileTest, ShelfPartyToggled) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(shelf_model()->in_shelf_party());
  EXPECT_FALSE(qs_tile_button_view()->IsToggled());
  shelf_model()->ToggleShelfParty();
  EXPECT_TRUE(shelf_model()->in_shelf_party());
  EXPECT_TRUE(qs_tile_button_view()->IsToggled());
  shelf_model()->ToggleShelfParty();
  EXPECT_FALSE(shelf_model()->in_shelf_party());
  EXPECT_FALSE(qs_tile_button_view()->IsToggled());
}

}  // namespace
}  // namespace ash
