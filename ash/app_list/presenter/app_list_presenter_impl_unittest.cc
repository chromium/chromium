// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/presenter/app_list_presenter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/window_util.h"

namespace app_list {

namespace {

// Test stub for AppListPresenterDelegate
class AppListPresenterDelegateTest : public AppListPresenterDelegate {
 public:
  explicit AppListPresenterDelegateTest(aura::Window* container)
      : container_(container) {}
  ~AppListPresenterDelegateTest() override {}

  bool init_called() const { return init_called_; }
  bool on_shown_called() const { return on_shown_called_; }
  bool on_dismissed_called() const { return on_dismissed_called_; }

  AppListViewDelegate* GetAppListViewDelegate() override {
    return &app_list_view_delegate_;
  }

 private:
  // AppListPresenterDelegate:
  void SetPresenter(AppListPresenterImpl* presenter) override {
    presenter_ = presenter;
  }
  void Init(AppListView* view,
            int64_t display_id,
            int current_apps_page) override {
    init_called_ = true;
    view_ = view;
    AppListView::InitParams params;
    params.parent = container_;
    params.initial_apps_page = current_apps_page;
    view->Initialize(params);
  }
  void OnShown(int64_t display_id) override { on_shown_called_ = true; }
  void OnClosing() override { on_dismissed_called_ = true; }
  void OnClosed() override {}
  gfx::Vector2d GetVisibilityAnimationOffset(aura::Window*) override {
    return gfx::Vector2d(0, 0);
  }
  base::TimeDelta GetVisibilityAnimationDuration(aura::Window* root_window,
                                                 bool is_visible) override {
    return base::TimeDelta::FromMilliseconds(0);
  }
  bool IsHomeLauncherEnabledInTabletMode() override { return false; }
  bool GetOnScreenKeyboardShown() override { return false; }
  aura::Window* GetRootWindowForDisplayId(int64_t display_id) override {
    return nullptr;
  }
  void OnVisibilityChanged(bool visible, aura::Window* root_window) override {}
  void OnTargetVisibilityChanged(bool visible) override {}

 private:
  aura::Window* container_;
  AppListPresenterImpl* presenter_ = nullptr;
  AppListView* view_ = nullptr;
  bool init_called_ = false;
  bool on_shown_called_ = false;
  bool on_dismissed_called_ = false;
  views::TestViewsDelegate view_delegate_;
  test::AppListTestViewDelegate app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

}  // namespace

class AppListPresenterImplTest : public aura::test::AuraTestBase {
 public:
  AppListPresenterImplTest();
  ~AppListPresenterImplTest() override;

  AppListPresenterImpl* presenter() { return presenter_.get(); }
  aura::Window* container() { return container_.get(); }
  int64_t GetDisplayId() { return test_screen()->GetPrimaryDisplay().id(); }
  AppListPresenterDelegateTest* delegate() { return presenter_delegate_; }

  // aura::test::AuraTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<AppListPresenterImpl> presenter_;
  AppListPresenterDelegateTest* presenter_delegate_ = nullptr;
  std::unique_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImplTest);
};

AppListPresenterImplTest::AppListPresenterImplTest() {}

AppListPresenterImplTest::~AppListPresenterImplTest() {}

void AppListPresenterImplTest::SetUp() {
  AuraTestBase::SetUp();
  new wm::DefaultActivationClient(root_window());
  container_.reset(CreateNormalWindow(0, root_window(), nullptr));
  std::unique_ptr<AppListPresenterDelegateTest> presenter_delegate =
      std::make_unique<AppListPresenterDelegateTest>(container_.get());
  presenter_delegate_ = presenter_delegate.get();
  presenter_ =
      std::make_unique<AppListPresenterImpl>(std::move(presenter_delegate));
}

void AppListPresenterImplTest::TearDown() {
  container_.reset();
  AuraTestBase::TearDown();
}

// Tests that app launcher is dismissed when focus moves to a window which is
// not app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown and then when the app launcher is
// dismissed.
TEST_F(AppListPresenterImplTest, HideOnFocusOut) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  presenter()->Show(GetDisplayId(), base::TimeTicks());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  focus_client->FocusWindow(presenter()->GetWindow());
  EXPECT_TRUE(presenter()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, root_window(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(delegate()->on_dismissed_called());
  EXPECT_FALSE(presenter()->GetTargetVisibility());
}

// Tests that app launcher remains visible when focus moves to a window which
// is app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown.
TEST_F(AppListPresenterImplTest, RemainVisibleWhenFocusingToSibling) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  presenter()->Show(GetDisplayId(), base::TimeTicks());
  focus_client->FocusWindow(presenter()->GetWindow());
  EXPECT_TRUE(presenter()->GetTargetVisibility());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());

  // Create a sibling window.
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, container(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(presenter()->GetTargetVisibility());
  EXPECT_FALSE(delegate()->on_dismissed_called());
}

// Tests that the app list is dismissed but the delegate is still active when
// the app list's widget is destroyed.
TEST_F(AppListPresenterImplTest, WidgetDestroyed) {
  presenter()->Show(GetDisplayId(), base::TimeTicks());
  EXPECT_TRUE(presenter()->GetTargetVisibility());
  presenter()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(presenter()->GetTargetVisibility());
  EXPECT_TRUE(delegate());
}

// Test that clicking on app list context menus doesn't close the app list.
TEST_F(AppListPresenterImplTest, ClickingContextMenuDoesNotDismiss) {
  // Populate some apps since we will show the context menu over a view.
  test::AppListTestViewDelegate* view_delegate =
      static_cast<test::AppListTestViewDelegate*>(
          delegate()->GetAppListViewDelegate());
  view_delegate->GetTestModel()->PopulateApps(2);

  // Show the app list on the primary display.
  presenter()->Show(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                    base::TimeTicks());
  aura::Window* window = presenter()->GetWindow();
  ASSERT_TRUE(window);

  // Show a context menu for the first app list item view.
  app_list::AppListView::TestApi test_api(presenter()->GetView());
  app_list::AppsGridView* grid_view = test_api.GetRootAppsGridView();
  app_list::AppListItemView* item_view = grid_view->GetItemViewAt(0);
  DCHECK(item_view);
  item_view->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  // Find the context menu as a transient child of the app list.
  aura::Window* transient_parent = window;
  const std::vector<aura::Window*>& transient_children =
      wm::GetTransientChildren(transient_parent);
  ASSERT_EQ(1u, transient_children.size());
  aura::Window* menu = transient_children[0];

  // Press the left mouse button on the menu window, it should not close the
  // app list nor the context menu on this pointer event.
  ui::test::EventGenerator menu_event_generator(menu);
  menu_event_generator.set_current_location(menu->GetBoundsInScreen().origin());
  menu_event_generator.PressLeftButton();

  // Check that the window and the app list are still open.
  ASSERT_EQ(window, presenter()->GetWindow());
  EXPECT_EQ(1u, wm::GetTransientChildren(transient_parent).size());

  // Close app list so that views are destructed and unregistered from the
  // model's observer list.
  presenter()->Dismiss(base::TimeTicks());
  base::RunLoop().RunUntilIdle();
}

}  // namespace app_list
