// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
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
  raw_ptr<ShelfView, DanglingUntriaged> shelf_view_ = nullptr;
  raw_ptr<ShelfModel, DanglingUntriaged> shelf_model_ = nullptr;
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

TEST_F(ShelfTest, AppIconInkDropBaseColor) {
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
  EXPECT_EQ(button->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysRippleNeutralOnSubtle),
            views::InkDrop::Get(button)->GetBaseColor());
  EXPECT_EQ(1.0f, views::InkDrop::Get(button)->GetVisibleOpacity());
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
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
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

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
}

// Various assertions around disabling auto-hide.
TEST_F(ShelfTest, DisableAutoHide) {
  // Create and activate a `window`.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();
  wm::ActivateWindow(window.get());

  // Set `shelf` to always auto-hide.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Verify `shelf` is auto-hidden.
  ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  EXPECT_TRUE(shelf_layout_manager->is_shelf_auto_hidden());

  {
    // Verify that auto-hide can be disabled using `ScopedDisableAutoHide`.
    Shelf::ScopedDisableAutoHide disable_auto_hide(shelf);
    EXPECT_FALSE(shelf_layout_manager->is_shelf_auto_hidden());
  }

  // Verify `shelf` is auto-hidden.
  EXPECT_TRUE(shelf_layout_manager->is_shelf_auto_hidden());

  // Lock shelf in auto-hidden state.
  Shelf::ScopedAutoHideLock auto_hide_lock(shelf);
  EXPECT_TRUE(shelf_layout_manager->is_shelf_auto_hidden());

  {
    // Verify that auto-hide cannot be disabled using `ScopedDisableAutoHide`.
    Shelf::ScopedDisableAutoHide disable_auto_hide(shelf);
    EXPECT_TRUE(shelf_layout_manager->is_shelf_auto_hidden());
  }

  // Verify `shelf` is auto-hidden.
  EXPECT_TRUE(shelf_layout_manager->is_shelf_auto_hidden());
}

// Tests if shelf is hidden on secondary display after the primary display is
// changed.
TEST_F(ShelfTest, ShelfHiddenOnScreenOnSecondaryDisplay) {
  for (const auto& state : {session_manager::SessionState::LOCKED,
                            session_manager::SessionState::LOGIN_PRIMARY}) {
    SCOPED_TRACE(
        base::StringPrintf("Testing state: %d", static_cast<int>(state)));
    GetSessionControllerClient()->SetSessionState(state);
    UpdateDisplay("800x600,800x600");

    EXPECT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
    EXPECT_EQ(SHELF_HIDDEN, GetSecondaryShelf()->GetVisibilityState());

    SwapPrimaryDisplay();

    EXPECT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
    EXPECT_EQ(SHELF_HIDDEN, GetSecondaryShelf()->GetVisibilityState());
  }
}

TEST_F(ShelfTest, ShelfNavigationWidgetAccessibleProperties) {
  ShelfNavigationWidget::TestApi widget_api(
      GetPrimaryShelf()->navigation_widget());
  ui::AXNodeData data;

  widget_api.GetWidgetDelegateView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kToolbar, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME),
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
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

}  // namespace
}  // namespace ash
