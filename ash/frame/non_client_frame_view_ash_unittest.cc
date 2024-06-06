// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using ::chromeos::DefaultFrameHeader;
using ::chromeos::FrameCaptionButtonContainerView;
using ::chromeos::ImmersiveFullscreenController;
using ::chromeos::ImmersiveFullscreenControllerDelegate;
using ::chromeos::ImmersiveFullscreenControllerTestApi;
using ::chromeos::kFrameActiveColorKey;
using ::chromeos::kFrameInactiveColorKey;
using ::chromeos::kTrackDefaultFrameColors;

// A views::WidgetDelegate which uses a NonClientFrameViewAsh.
class NonClientFrameViewAshTestWidgetDelegate
    : public views::WidgetDelegateView {
 public:
  NonClientFrameViewAshTestWidgetDelegate() = default;

  NonClientFrameViewAshTestWidgetDelegate(
      const NonClientFrameViewAshTestWidgetDelegate&) = delete;
  NonClientFrameViewAshTestWidgetDelegate& operator=(
      const NonClientFrameViewAshTestWidgetDelegate&) = delete;

  ~NonClientFrameViewAshTestWidgetDelegate() override = default;

  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    auto non_client_frame_view =
        std::make_unique<NonClientFrameViewAsh>(widget);
    non_client_frame_view_ = non_client_frame_view.get();
    return non_client_frame_view;
  }

  int GetNonClientFrameViewTopBorderHeight() {
    return non_client_frame_view_->NonClientTopBorderHeight();
  }

  NonClientFrameViewAsh* non_client_frame_view() const {
    return non_client_frame_view_;
  }

  chromeos::HeaderView* header_view() const {
    return non_client_frame_view_->GetHeaderView();
  }

 private:
  // Not owned.
  raw_ptr<NonClientFrameViewAsh> non_client_frame_view_ = nullptr;
};

class TestWidgetConstraintsDelegate
    : public NonClientFrameViewAshTestWidgetDelegate {
 public:
  TestWidgetConstraintsDelegate() {
    SetCanMaximize(true);
    SetCanMinimize(true);
  }

  TestWidgetConstraintsDelegate(const TestWidgetConstraintsDelegate&) = delete;
  TestWidgetConstraintsDelegate& operator=(
      const TestWidgetConstraintsDelegate&) = delete;

  ~TestWidgetConstraintsDelegate() override = default;

  // views::View:
  gfx::Size GetMinimumSize() const override { return minimum_size_; }

  gfx::Size GetMaximumSize() const override { return maximum_size_; }

  views::View* GetContentsView() override {
    // Set this instance as the contents view so that the maximum and minimum
    // size constraints will be used.
    return this;
  }

  // views::WidgetDelegate:
  void set_minimum_size(const gfx::Size& min_size) { minimum_size_ = min_size; }

  void set_maximum_size(const gfx::Size& max_size) { maximum_size_ = max_size; }

  const gfx::Rect& GetFrameCaptionButtonContainerViewBounds() {
    return non_client_frame_view()
        ->GetFrameCaptionButtonContainerViewForTest()
        ->bounds();
  }

  void EndFrameCaptionButtonContainerViewAnimations() {
    FrameCaptionButtonContainerView::TestApi test(
        non_client_frame_view()->GetFrameCaptionButtonContainerViewForTest());
    test.EndAnimations();
  }

  int GetTitleBarHeight() const {
    return non_client_frame_view()->NonClientTopBorderHeight();
  }

 private:
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;
};

using NonClientFrameViewAshTest = AshTestBase;

// Verifies the client view is not placed at a y location of 0.
TEST_F(NonClientFrameViewAshTest, ClientViewCorrectlyPlaced) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       new NonClientFrameViewAshTestWidgetDelegate);
  EXPECT_NE(0, widget->client_view()->bounds().y());
}

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(NonClientFrameViewAshTest, HeaderHeight) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  // The header should have enough room for the window controls. The
  // header/content separator line overlays the window controls.
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->non_client_frame_view()->GetHeaderView()->height());
}

// Regression test for https://crbug.com/839955
TEST_F(NonClientFrameViewAshTest, ActiveStateOfButtonMatchesWidget) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  FrameCaptionButtonContainerView::TestApi test_api(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());

  widget->Show();
  EXPECT_TRUE(widget->IsActive());
  // The paint state doesn't change till the next paint.
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  EXPECT_TRUE(test_api.size_button()->GetPaintAsActive());

  // Activate a different widget so the original one loses activation.
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       new NonClientFrameViewAshTestWidgetDelegate);
  widget2->Show();
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(test_api.size_button()->GetPaintAsActive());
}

// Verify that NonClientFrameViewAsh returns the correct minimum and maximum
// frame sizes when the client view does not specify any size constraints.
TEST_F(NonClientFrameViewAshTest, NoSizeConstraints) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  gfx::Size min_frame_size = non_client_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = non_client_frame_view->GetMaximumSize();

  EXPECT_EQ(delegate->GetTitleBarHeight(), min_frame_size.height());

  // A width and height constraint of 0 denotes unbounded.
  EXPECT_EQ(0, max_frame_size.width());
  EXPECT_EQ(0, max_frame_size.height());
}

// Verify that NonClientFrameViewAsh returns the correct minimum and maximum
// frame sizes when the client view specifies size constraints.
TEST_F(NonClientFrameViewAshTest, MinimumAndMaximumSize) {
  gfx::Size min_client_size(500, 500);
  gfx::Size max_client_size(800, 800);
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  delegate->set_minimum_size(min_client_size);
  delegate->set_maximum_size(max_client_size);
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  gfx::Size min_frame_size = non_client_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = non_client_frame_view->GetMaximumSize();

  EXPECT_EQ(min_client_size.width(), min_frame_size.width());
  EXPECT_EQ(max_client_size.width(), max_frame_size.width());
  EXPECT_EQ(min_client_size.height() + delegate->GetTitleBarHeight(),
            min_frame_size.height());
  EXPECT_EQ(max_client_size.height() + delegate->GetTitleBarHeight(),
            max_frame_size.height());
}

// Verify that NonClientFrameViewAsh updates the avatar icon based on the
// avatar icon window property.
TEST_F(NonClientFrameViewAshTest, AvatarIcon) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  EXPECT_FALSE(non_client_frame_view->GetAvatarIconViewForTest());

  // Avatar image becomes available.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kAvatarIconKey,
      new gfx::ImageSkia(gfx::test::CreateImage(27, 27).AsImageSkia()));
  EXPECT_TRUE(non_client_frame_view->GetAvatarIconViewForTest());

  // Avatar image is gone; the ImageView for the avatar icon should be
  // removed.
  widget->GetNativeWindow()->ClearProperty(aura::client::kAvatarIconKey);
  EXPECT_FALSE(non_client_frame_view->GetAvatarIconViewForTest());
}

// Tests that a window is minimized, toggling tablet mode doesn't trigger
// caption button update (https://crbug.com/822890).
TEST_F(NonClientFrameViewAshTest, ToggleTabletModeOnMinimizedWindow) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  FrameCaptionButtonContainerView::TestApi test(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());
  widget->Maximize();

  // Restore icon for size button in maximized window state. Compare by name
  // because the address may not be the same for different build targets in the
  // component build.
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test.size_button()->icon_definition_for_test()->name);
  widget->Minimize();

  // Enter and exit tablet mode while the window is minimized.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ash::TabletModeControllerTestApi().LeaveTabletMode();

  // When unminimizing in non-tablet mode, size button should match with
  // maximized window state, which is restore icon.
  ::wm::Unminimize(widget->GetNativeWindow());
  EXPECT_TRUE(widget->IsMaximized());
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test.size_button()->icon_definition_for_test()->name);
}

// Verify that when in tablet mode with a maximized window, the height of the
// header is zero.
TEST_F(NonClientFrameViewAshTest, FrameHiddenInTabletModeForMaximizedWindows) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  widget->Maximize();

  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
}

// Verify that when in tablet mode with a non maximized window, the height of
// the header is non zero.
TEST_F(NonClientFrameViewAshTest,
       FrameShownInTabletModeForNonMaximizedWindows) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->GetNonClientFrameViewTopBorderHeight());
}

// Verify that if originally in fullscreen mode, and enter tablet mode, the
// height of the header remains zero.
TEST_F(NonClientFrameViewAshTest,
       FrameRemainsHiddenInTabletModeWhenTogglingFullscreen) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  widget->SetFullscreen(true);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
}

TEST_F(NonClientFrameViewAshTest, OpeningAppsInTabletMode) {
  auto* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  widget->Maximize();

  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  // Verify that after minimizing and showing the widget, the height of the
  // header is zero.
  widget->Minimize();
  widget->Show();
  EXPECT_TRUE(widget->IsMaximized());
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  // Verify that when we toggle maximize, the header is shown. For example,
  // maximized can be toggled in tablet mode by using the accessibility
  // keyboard.
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(widget->GetNativeWindow())->OnWMEvent(&event);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->GetNonClientFrameViewTopBorderHeight());
}

// Regression test for https://b/331238593. See bug for more details.
TEST_F(NonClientFrameViewAshTest,
       NoCrashOnTabletChangesWithinWindowDestruction) {
  class WindowTestObserver : public aura::WindowObserver {
   public:
    explicit WindowTestObserver(aura::Window* window) {
      window_observation_.Observe(window);
    }
    ~WindowTestObserver() override = default;

    void OnWindowDestroying(aura::Window* window) override {
      // Simulate a tablet state change from within window destruction. It's not
      // clear how this may happen in production, but it triggers the same
      // reported crash stack.
      TabletModeControllerTestApi().EnterTabletMode();
      window_observation_.Reset();
    }

   private:
    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observation_{this};
  };

  auto test_window = CreateTestWindow(gfx::Rect(200, 200));
  WindowTestObserver obs(test_window.get());
  test_window.reset();
}

// Test if creating a new window in tablet mode uses maximzied state
// and immersive mode.
TEST_F(NonClientFrameViewAshTest, GetPreferredOnScreenHeightInTabletMaximzied) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  auto* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  auto* frame_view = static_cast<NonClientFrameViewAsh*>(
      widget->non_client_view()->frame_view());
  auto* header_view = frame_view->GetHeaderView();
  ASSERT_TRUE(widget->IsMaximized());
  EXPECT_TRUE(header_view->in_immersive_mode());
  static_cast<ImmersiveFullscreenControllerDelegate*>(header_view)
      ->SetVisibleFraction(0.5);
  // The height should be ~(33 *.5)
  EXPECT_NEAR(16, header_view->GetPreferredOnScreenHeight(), 1);
  static_cast<ImmersiveFullscreenControllerDelegate*>(header_view)
      ->SetVisibleFraction(0.0);
  EXPECT_EQ(0, header_view->GetPreferredOnScreenHeight());
}

// Verify windows that are minimized and then entered into tablet mode will have
// no header when unminimized in tablet mode.
TEST_F(NonClientFrameViewAshTest, MinimizedWindowsInTabletMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       new NonClientFrameViewAshTestWidgetDelegate);
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize);
  widget->Maximize();
  widget->Minimize();
  ash::TabletModeControllerTestApi().EnterTabletMode();

  widget->Show();
  EXPECT_EQ(widget->non_client_view()->bounds(),
            widget->client_view()->bounds());
}

TEST_F(NonClientFrameViewAshTest, HeaderVisibilityInFullscreen) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  auto* controller = ImmersiveFullscreenController::Get(widget.get());
  ImmersiveFullscreenControllerTestApi test_api(controller);
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  chromeos::HeaderView* header_view = non_client_frame_view->GetHeaderView();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());

  widget->SetFullscreen(true);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  test_api.EndAnimation();
  EXPECT_FALSE(header_view->GetVisible());

  widget->SetFullscreen(false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  test_api.EndAnimation();
  EXPECT_TRUE(header_view->GetVisible());

  // Turn immersive off, and make sure that header view is invisible
  // in fullscreen.
  widget->SetFullscreen(true);
  ImmersiveFullscreenController::EnableForWidget(widget.get(), false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_FALSE(header_view->GetVisible());
  widget->SetFullscreen(false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
}

namespace {

class TestButtonModel : public chromeos::CaptionButtonModel {
 public:
  TestButtonModel() = default;

  TestButtonModel(const TestButtonModel&) = delete;
  TestButtonModel& operator=(const TestButtonModel&) = delete;

  ~TestButtonModel() override = default;

  void set_zoom_mode(bool zoom_mode) { zoom_mode_ = zoom_mode; }

  void SetVisible(views::CaptionButtonIcon type, bool visible) {
    if (visible)
      visible_buttons_.insert(type);
    else
      visible_buttons_.erase(type);
  }

  void SetEnabled(views::CaptionButtonIcon type, bool enabled) {
    if (enabled)
      enabled_buttons_.insert(type);
    else
      enabled_buttons_.erase(type);
  }

  // CaptionButtonModel::
  bool IsVisible(views::CaptionButtonIcon type) const override {
    return visible_buttons_.count(type);
  }
  bool IsEnabled(views::CaptionButtonIcon type) const override {
    return enabled_buttons_.count(type);
  }
  bool InZoomMode() const override { return zoom_mode_; }

 private:
  base::flat_set<views::CaptionButtonIcon> visible_buttons_;
  base::flat_set<views::CaptionButtonIcon> enabled_buttons_;
  bool zoom_mode_ = false;
};

}  // namespace

TEST_F(NonClientFrameViewAshTest, BackButton) {
  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  std::unique_ptr<TestButtonModel> model = std::make_unique<TestButtonModel>();
  TestButtonModel* model_ptr = model.get();

  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 400, 500));

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  ui::TestAcceleratorTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::TestAcceleratorTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  non_client_frame_view->SetCaptionButtonModel(std::move(model));

  chromeos::HeaderView* header_view = non_client_frame_view->GetHeaderView();
  EXPECT_FALSE(header_view->GetBackButton());
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton());
  EXPECT_FALSE(header_view->GetBackButton()->GetEnabled());

  // Back button is disabled, so clicking on it should not should
  // generate back key sequence.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      header_view->GetBackButton()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton());
  EXPECT_TRUE(header_view->GetBackButton()->GetEnabled());

  // Back button is now enabled, so clicking on it should generate
  // back key sequence.
  generator->MoveMouseTo(
      header_view->GetBackButton()->GetBoundsInScreen().CenterPoint());

  generator->ClickLeftButton();
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, false);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->GetBackButton());
}

// Make sure that client view occupies the entire window when the
// frame is hidden.
TEST_F(NonClientFrameViewAshTest, FrameVisibility) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  gfx::Rect window_bounds(10, 10, 200, 100);
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate,
      desks_util::GetActiveDeskContainerId(), window_bounds);

  // The height is smaller by the top border height.
  gfx::Size client_bounds(200, 68);
  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  EXPECT_EQ(client_bounds, widget->client_view()->GetLocalBounds().size());

  non_client_frame_view->SetFrameEnabled(false);
  views::test::RunScheduledLayout(widget->GetRootView());
  EXPECT_EQ(gfx::Size(200, 100),
            widget->client_view()->GetLocalBounds().size());
  EXPECT_FALSE(non_client_frame_view->GetFrameEnabled());
  EXPECT_EQ(
      window_bounds,
      non_client_frame_view->GetClientBoundsForWindowBounds(window_bounds));

  non_client_frame_view->SetFrameEnabled(true);
  views::test::RunScheduledLayout(widget->GetRootView());
  EXPECT_EQ(client_bounds, widget->client_view()->GetLocalBounds().size());
  EXPECT_TRUE(non_client_frame_view->GetFrameEnabled());
  EXPECT_EQ(32, delegate->GetNonClientFrameViewTopBorderHeight());
  EXPECT_EQ(
      gfx::Rect(gfx::Point(10, 42), client_bounds),
      non_client_frame_view->GetClientBoundsForWindowBounds(window_bounds));
}

TEST_F(NonClientFrameViewAshTest, CustomButtonModel) {
  std::unique_ptr<TestButtonModel> model = std::make_unique<TestButtonModel>();
  TestButtonModel* model_ptr = model.get();

  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  non_client_frame_view->SetCaptionButtonModel(std::move(model));

  chromeos::HeaderView* header_view = non_client_frame_view->GetHeaderView();
  FrameCaptionButtonContainerView::TestApi test_api(
      header_view->caption_button_container());

  EXPECT_FALSE(test_api.close_button()->GetVisible());
  EXPECT_FALSE(test_api.minimize_button()->GetVisible());
  EXPECT_FALSE(test_api.size_button()->GetVisible());
  EXPECT_FALSE(test_api.menu_button()->GetVisible());

  // Close button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_CLOSE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.close_button()->GetVisible());
  EXPECT_FALSE(test_api.close_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_CLOSE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.close_button()->GetEnabled());

  // Back button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton()->GetVisible());
  EXPECT_FALSE(header_view->GetBackButton()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton()->GetEnabled());

  // size button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.size_button()->GetVisible());
  EXPECT_FALSE(test_api.size_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.size_button()->GetEnabled());

  // minimize button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MINIMIZE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.minimize_button()->GetVisible());
  EXPECT_FALSE(test_api.minimize_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MINIMIZE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.minimize_button()->GetEnabled());

  // menu button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MENU, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.menu_button()->GetVisible());
  EXPECT_FALSE(test_api.menu_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MENU, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.menu_button()->GetEnabled());

  // zoom button
  EXPECT_STREQ(views::kWindowControlMaximizeIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  model_ptr->set_zoom_mode(true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_STREQ(chromeos::kWindowControlZoomIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  widget->Maximize();
  EXPECT_STREQ(chromeos::kWindowControlDezoomIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
}

TEST_F(NonClientFrameViewAshTest, WideFrame) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(100, 0, 400, 500));

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  chromeos::HeaderView* header_view = non_client_frame_view->GetHeaderView();
  widget->Maximize();

  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  wide_frame_view->GetWidget()->Show();

  chromeos::HeaderView* wide_header_view = wide_frame_view->header_view();
  display::Screen* screen = display::Screen::GetScreen();

  const gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  gfx::Rect frame_bounds =
      wide_frame_view->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(work_area.width(), frame_bounds.width());
  EXPECT_EQ(work_area.origin(), frame_bounds.origin());
  EXPECT_FALSE(header_view->should_paint());
  EXPECT_TRUE(wide_header_view->should_paint());

  // Test immersive.
  ImmersiveFullscreenController controller;
  wide_frame_view->Init(&controller);
  EXPECT_FALSE(wide_header_view->in_immersive_mode());
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());

  ImmersiveFullscreenController::EnableForWidget(widget.get(), true);
  EXPECT_TRUE(header_view->in_immersive_mode());
  EXPECT_TRUE(wide_header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  // The height should be ~(33 *.5)
  wide_header_view->SetVisibleFraction(0.5);
  EXPECT_NEAR(16, wide_header_view->GetPreferredOnScreenHeight(), 1);

  // Make sure the frame can be revaled outside of the target window.
  EXPECT_FALSE(ImmersiveFullscreenControllerTestApi(&controller)
                   .IsTopEdgeHoverTimerRunning());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(10, 0));
  generator->MoveMouseBy(1, 0);
  EXPECT_TRUE(ImmersiveFullscreenControllerTestApi(&controller)
                  .IsTopEdgeHoverTimerRunning());

  generator->MoveMouseTo(gfx::Point(10, 10));
  generator->MoveMouseBy(1, 0);
  EXPECT_FALSE(ImmersiveFullscreenControllerTestApi(&controller)
                   .IsTopEdgeHoverTimerRunning());

  generator->MoveMouseTo(gfx::Point(600, 0));
  generator->MoveMouseBy(1, 0);
  EXPECT_TRUE(ImmersiveFullscreenControllerTestApi(&controller)
                  .IsTopEdgeHoverTimerRunning());

  ImmersiveFullscreenController::EnableForWidget(widget.get(), false);
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_FALSE(wide_header_view->in_immersive_mode());
  // visible fraction should be ignored in non immersive.
  wide_header_view->SetVisibleFraction(0.5);
  EXPECT_EQ(32, wide_header_view->GetPreferredOnScreenHeight());

  UpdateDisplay("1234x800");
  EXPECT_EQ(1234,
            wide_frame_view->GetWidget()->GetWindowBoundsInScreen().width());

  // Double Click
  EXPECT_TRUE(widget->IsMaximized());
  generator->MoveMouseToCenterOf(
      wide_header_view->GetWidget()->GetNativeWindow());
  generator->DoubleClickLeftButton();
  EXPECT_FALSE(widget->IsMaximized());
}

TEST_F(NonClientFrameViewAshTest, WideFrameButton) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(100, 0, 400, 500));
  widget->Maximize();
  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  wide_frame_view->GetWidget()->Show();
  chromeos::HeaderView* header_view = wide_frame_view->header_view();
  FrameCaptionButtonContainerView::TestApi test_api(
      header_view->caption_button_container());

  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);

  widget->SetFullscreen(true);
  views::test::RunScheduledLayout(header_view);
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  {
    WMEvent event(WM_EVENT_PIN);
    WindowState::Get(widget->GetNativeWindow())->OnWMEvent(&event);
    views::test::RunScheduledLayout(header_view);
    EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
                 test_api.size_button()->icon_definition_for_test()->name);
  }
  {
    WMEvent event(WM_EVENT_TRUSTED_PIN);
    WindowState::Get(widget->GetNativeWindow())->OnWMEvent(&event);
    views::test::RunScheduledLayout(header_view);
    EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
                 test_api.size_button()->icon_definition_for_test()->name);
  }
}

TEST_F(NonClientFrameViewAshTest, MoveFullscreenWideFrameBetweenDisplay) {
  UpdateDisplay("800x600, 1000x600");

  auto* screen = display::Screen::GetScreen();
  auto display_list = screen->GetAllDisplays();

  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(100, 0, 400, 500));
  widget->SetFullscreen(true);
  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  wide_frame_view->GetWidget()->Show();
  ASSERT_EQ(display_list[0].id(),
            screen->GetDisplayNearestWindow(widget->GetNativeWindow()).id());
  EXPECT_EQ(800,
            wide_frame_view->GetWidget()->GetWindowBoundsInScreen().width());

  window_util::MoveWindowToDisplay(widget->GetNativeWindow(),
                                   display_list[1].id());
  EXPECT_EQ(display_list[1].id(),
            screen->GetDisplayNearestWindow(widget->GetNativeWindow()).id());
  EXPECT_EQ(1000,
            wide_frame_view->GetWidget()->GetWindowBoundsInScreen().width());
}

namespace {

class NonClientFrameViewAshFrameColorTest
    : public NonClientFrameViewAshTest,
      public testing::WithParamInterface<bool> {
 public:
  NonClientFrameViewAshFrameColorTest() = default;

  NonClientFrameViewAshFrameColorTest(
      const NonClientFrameViewAshFrameColorTest&) = delete;
  NonClientFrameViewAshFrameColorTest& operator=(
      const NonClientFrameViewAshFrameColorTest&) = delete;

  ~NonClientFrameViewAshFrameColorTest() override = default;
};

class TestWidgetDelegate : public TestWidgetConstraintsDelegate {
 public:
  TestWidgetDelegate(bool custom) : custom_(custom) {}

  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate& operator=(const TestWidgetDelegate&) = delete;

  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    if (custom_) {
      WindowState* window_state = WindowState::Get(widget->GetNativeWindow());
      window_state->SetDelegate(std::make_unique<WindowStateDelegate>());
    }
    return TestWidgetConstraintsDelegate::CreateNonClientFrameView(widget);
  }

 private:
  bool custom_;
};

}  // namespace

// Verify that NonClientFrameViewAsh updates the active color based on the
// kFrameActiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, kFrameActiveColorKey) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  SkColor active_color =
      widget->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
  constexpr SkColor new_color = SK_ColorWHITE;
  EXPECT_NE(active_color, new_color);

  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, new_color);
  active_color = widget->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
  EXPECT_EQ(active_color, new_color);
  EXPECT_EQ(new_color,
            delegate->non_client_frame_view()->GetActiveFrameColorForTest());

  // Test that changing the property updates the caption button images.
  FrameCaptionButtonContainerView::TestApi test_api(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  gfx::ImageSkia original_icon_image = test_api.size_button()->icon_image();
  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, SK_ColorBLACK);
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  EXPECT_FALSE(original_icon_image.BackedBySameObjectAs(
      test_api.size_button()->icon_image()));
}

// Verify that NonClientFrameViewAsh updates the inactive color based on the
// kFrameInactiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, KFrameInactiveColor) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);

  SkColor active_color =
      widget->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
  constexpr SkColor new_color = SK_ColorWHITE;
  EXPECT_NE(active_color, new_color);

  widget->GetNativeWindow()->SetProperty(kFrameInactiveColorKey, new_color);
  active_color = widget->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
  EXPECT_EQ(active_color, new_color);
  EXPECT_EQ(new_color,
            delegate->non_client_frame_view()->GetInactiveFrameColorForTest());
}

// Verify that NonClientFrameViewAsh updates the active and inactive colors at
// construction.
TEST_P(NonClientFrameViewAshFrameColorTest, KFrameColorCtor) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  // Build the window, this implicit constructs the NonClientFrameView.
  constexpr SkColor non_default_color = SK_ColorWHITE;
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder()
          .SetDelegate(delegate)
          .SetBounds(gfx::Rect())
          .SetParent(Shell::GetPrimaryRootWindow()->GetChildById(
              desks_util::GetActiveDeskContainerId()))
          .SetShow(true)
          .SetWindowProperty(kTrackDefaultFrameColors, false)
          .SetWindowProperty(kFrameActiveColorKey, non_default_color)
          .SetWindowProperty(kFrameInactiveColorKey, non_default_color)
          .BuildOwnsNativeWidget();

  // Check that the default color is different from the one used in the  test.
  SkColor inactive_color =
      widget->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
  SkColor active_color =
      widget->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
  EXPECT_EQ(active_color, non_default_color);
  EXPECT_EQ(inactive_color, non_default_color);
  EXPECT_EQ(delegate->non_client_frame_view()->GetInactiveFrameColorForTest(),
            non_default_color);
  EXPECT_EQ(delegate->non_client_frame_view()->GetActiveFrameColorForTest(),
            non_default_color);
}

// Verify that NonClientFrameViewAsh updates the active color based on the
// kFrameActiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, WideFrameInitialColor) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  widget->Maximize();
  aura::Window* window = widget->GetNativeWindow();
  SkColor active_color = window->GetProperty(kFrameActiveColorKey);
  SkColor inactive_color = window->GetProperty(kFrameInactiveColorKey);
  constexpr SkColor new_active_color = SK_ColorWHITE;
  constexpr SkColor new_inactive_color = SK_ColorBLACK;
  EXPECT_NE(active_color, new_active_color);
  EXPECT_NE(inactive_color, new_inactive_color);
  window->SetProperty(kFrameActiveColorKey, new_active_color);
  window->SetProperty(kFrameInactiveColorKey, new_inactive_color);

  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  chromeos::HeaderView* wide_header_view = wide_frame_view->header_view();
  DefaultFrameHeader* header = wide_header_view->GetFrameHeader();
  EXPECT_EQ(new_active_color, header->active_frame_color_);
  EXPECT_EQ(new_inactive_color, header->inactive_frame_color_);
}

// Tests to make sure that the NonClientFrameViewAsh tracks default frame colors
// for both light and dark mode.
TEST_P(NonClientFrameViewAshFrameColorTest, DefaultFrameColorsDarkAndLight) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  aura::Window* window = widget->GetNativeWindow();

  auto* color_provider = delegate->non_client_frame_view()->GetColorProvider();
  SkColor dialog_title_bar_color =
      color_provider->GetColor(cros_tokens::kDialogTitleBarColor);
  const SkColor initial_active_default = dialog_title_bar_color;
  const SkColor initial_inactive_default = dialog_title_bar_color;
  SkColor active_color = window->GetProperty(kFrameActiveColorKey);
  SkColor inactive_color = window->GetProperty(kFrameInactiveColorKey);

  EXPECT_EQ(initial_active_default, active_color);
  EXPECT_EQ(initial_inactive_default, inactive_color);

  // Switch the color mode
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());
  // Get the `color_provider` again as it might have changed because of the
  // color mode change.
  color_provider = delegate->non_client_frame_view()->GetColorProvider();
  dialog_title_bar_color =
      color_provider->GetColor(cros_tokens::kDialogTitleBarColor);

  const SkColor active_default = dialog_title_bar_color;
  const SkColor inactive_default = dialog_title_bar_color;
  active_color = window->GetProperty(kFrameActiveColorKey);
  inactive_color = window->GetProperty(kFrameInactiveColorKey);

  EXPECT_NE(initial_active_default, active_default);
  EXPECT_NE(initial_inactive_default, inactive_default);
  EXPECT_EQ(active_default, active_color);
  EXPECT_EQ(inactive_default, inactive_color);
}

// Tests to make sure that NonClientFrameViewAsh does not clobber custom frame
// colors when the kTrackDefaultFrameColors property is set to false.
TEST_P(NonClientFrameViewAshFrameColorTest,
       CanSetPersistentFrameColorsDarkAndLight) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, delegate);
  aura::Window* window = widget->GetNativeWindow();

  constexpr SkColor new_active_color = SK_ColorWHITE;
  constexpr SkColor new_inactive_color = SK_ColorBLACK;

  EXPECT_NE(new_active_color, window->GetProperty(kFrameActiveColorKey));
  EXPECT_NE(new_inactive_color, window->GetProperty(kFrameInactiveColorKey));

  window->SetProperty(kTrackDefaultFrameColors, false);
  window->SetProperty(kFrameActiveColorKey, new_active_color);
  window->SetProperty(kFrameInactiveColorKey, new_inactive_color);

  EXPECT_EQ(new_active_color, window->GetProperty(kFrameActiveColorKey));
  EXPECT_EQ(new_inactive_color, window->GetProperty(kFrameInactiveColorKey));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(new_active_color, window->GetProperty(kFrameActiveColorKey));
  EXPECT_EQ(new_inactive_color, window->GetProperty(kFrameInactiveColorKey));
}

// Run frame color tests with and without custom WindowStateDelegate.
INSTANTIATE_TEST_SUITE_P(All,
                         NonClientFrameViewAshFrameColorTest,
                         testing::Bool());

}  // namespace ash
