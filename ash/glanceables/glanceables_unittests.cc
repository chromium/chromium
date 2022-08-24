// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_restore_view.h"
#include "ash/glanceables/glanceables_up_next_view.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/glanceables/test_glanceables_delegate.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/pill_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"

namespace ash {
namespace {

AmbientWeatherModel* GetWeatherModel() {
  return Shell::Get()->ambient_controller()->GetAmbientWeatherModel();
}

}  // namespace

// Unified test suite for the glanceables controller, views, etc.
class GlanceablesTest : public AshTestBase {
 public:
  GlanceablesTest() = default;
  ~GlanceablesTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->glanceables_controller();
    DCHECK(controller_);

    // Fake out the ambient backend controller so weather fetches won't crash.
    auto* ambient_controller = Shell::Get()->ambient_controller();
    // The controller must be null before a new instance can be created.
    ambient_controller->set_backend_controller_for_testing(nullptr);
    ambient_controller->set_backend_controller_for_testing(
        std::make_unique<FakeAmbientBackendControllerImpl>());
  }

  TestGlanceablesDelegate* GetTestDelegate() {
    return static_cast<TestGlanceablesDelegate*>(controller_->delegate_.get());
  }

  views::Widget* GetWidget() { return controller_->widget_.get(); }

  GlanceablesView* GetGlanceablesView() { return controller_->view_; }

  GlanceablesWelcomeLabel* GetWelcomeLabel() {
    return controller_->view_->welcome_label_;
  }

  views::ImageView* GetWeatherIcon() {
    return controller_->view_->weather_view_->icon_;
  }

  views::Label* GetWeatherTemperature() {
    return controller_->view_->weather_view_->temperature_;
  }

  GlanceablesUpNextView* GetUpNextView() {
    return controller_->view_->up_next_view_;
  }

  GlanceablesRestoreView* GetRestoreView() {
    return controller_->view_->restore_view_;
  }

 protected:
  GlanceablesController* controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesTest, CreateAndDestroyUi) {
  controller_->CreateUi();

  // A fullscreen widget was created.
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsFullscreen());

  // The controller's view is the widget's contents view.
  views::View* view = GetGlanceablesView();
  EXPECT_TRUE(view);
  EXPECT_EQ(view, widget->GetContentsView());

  controller_->DestroyUi();

  // Widget and glanceables view are destroyed.
  EXPECT_FALSE(GetWidget());
  EXPECT_FALSE(GetGlanceablesView());
}

TEST_F(GlanceablesTest, GlanceablesViewCreatesChildViews) {
  controller_->CreateUi();

  GlanceablesView* view = GetGlanceablesView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(GetWelcomeLabel());
  EXPECT_TRUE(GetWeatherIcon());
  EXPECT_TRUE(GetWeatherTemperature());
  EXPECT_TRUE(GetUpNextView());
  EXPECT_TRUE(GetRestoreView());
}

TEST_F(GlanceablesTest, WeatherViewShowsWeather) {
  controller_->CreateUi();

  // Icon starts blank.
  views::ImageView* icon = GetWeatherIcon();
  EXPECT_TRUE(icon->GetImage().isNull());

  // Trigger a weather update. Use an image the same size as the icon view's
  // image so the image won't be resized and we can compare backing objects.
  gfx::Rect image_bounds = icon->GetImageBounds();
  gfx::ImageSkia weather_image =
      gfx::test::CreateImageSkia(image_bounds.width(), image_bounds.height());
  GetWeatherModel()->UpdateWeatherInfo(weather_image, 72.0f,
                                       /*show_celsius=*/false);

  // The view reflects the new weather.
  EXPECT_EQ(weather_image.GetBackingObject(),
            icon->GetImage().GetBackingObject());
  EXPECT_EQ(u"72° F", GetWeatherTemperature()->GetText());
}

TEST_F(GlanceablesTest, UpNextViewRendersCorrectly) {
  controller_->CreateUi();

  // Events list contains rendered event items inside.
  const auto& items = GetUpNextView()->events_list_items_views_for_test();
  EXPECT_EQ(items.size(), 5u);
  for (const auto& item : items) {
    EXPECT_EQ(std::get<0>(item)->GetText(), u"James / Artsiom");
    EXPECT_EQ(std::get<1>(item)->GetText(), u"2:00 – 2:30pm");
  }
}

TEST_F(GlanceablesTest, ClickOnSessionRestore) {
  controller_->CreateUi();

  GlanceablesRestoreView* restore_view = GetRestoreView();
  ASSERT_TRUE(restore_view);
  ASSERT_EQ(0, GetTestDelegate()->restore_session_count());

  // Click on the restore view (which is a button).
  views::test::ButtonTestApi(restore_view).NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(1, GetTestDelegate()->restore_session_count());
}

TEST_F(GlanceablesTest, DismissesOnlyOnAppWindowOpen) {
  controller_->CreateUi();
  ASSERT_TRUE(controller_->IsShowing());

  // Showing the app list still shows glanceables.
  GetAppListTestHelper()->ShowAppList();
  EXPECT_TRUE(controller_->IsShowing());

  // Showing quick settings still shows glanceables.
  UnifiedSystemTray* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  tray->ActivateBubble();
  EXPECT_TRUE(controller_->IsShowing());

  // Creating an app window hides glanceables.
  std::unique_ptr<aura::Window> app_window = CreateAppWindow();
  EXPECT_FALSE(controller_->IsShowing());

  // Glanceables stay hidden after the app window is closed.
  app_window.reset();
  EXPECT_FALSE(controller_->IsShowing());
}

TEST_F(GlanceablesTest, ShowFromOverview) {
  ASSERT_FALSE(controller_->IsShowing());

  EnterOverview();
  const DesksBarView* desks_bar_view = GetPrimaryRootDesksBarView();
  auto* up_next_button = desks_bar_view->up_next_button();
  ASSERT_TRUE(up_next_button);

  LeftClickOn(up_next_button);

  // Glanceables are showing and overview mode is closed.
  EXPECT_TRUE(controller_->IsShowing());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(GlanceablesTest, ShowFromOverviewHidesAppWindows) {
  // Create windows, back to front.
  std::unique_ptr<aura::Window> back_window = CreateAppWindow();
  std::unique_ptr<aura::Window> middle_window = CreateAppWindow();
  std::unique_ptr<aura::Window> minimized_window = CreateAppWindow();
  WindowState::Get(minimized_window.get())->Minimize();
  std::unique_ptr<aura::Window> front_window = CreateAppWindow();

  controller_->ShowFromOverview();

  // All windows are minimized.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(middle_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(minimized_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsMinimized());

  // Destroy the middle window.
  middle_window.reset();

  // Hide glanceables.
  controller_->DestroyUi();

  // Front and back windows are restored.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsNormalStateType());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsNormalStateType());

  // The originally minimized window is still minimized.
  EXPECT_TRUE(WindowState::Get(minimized_window.get())->IsMinimized());

  // The front window is still frontmost (at the end of the child list).
  EXPECT_EQ(front_window->parent()->children().back(), front_window.get());
}

TEST_F(GlanceablesTest, UnminimizingOneWindowRestoresAllWindows) {
  std::unique_ptr<aura::Window> back_window = CreateAppWindow();
  std::unique_ptr<aura::Window> front_window = CreateAppWindow();

  controller_->ShowFromOverview();

  EXPECT_TRUE(WindowState::Get(back_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsMinimized());

  // Restore and activate the front window.
  WindowState::Get(front_window.get())->Unminimize();
  WindowState::Get(front_window.get())->Activate();

  // Window activation closed glanceables.
  EXPECT_FALSE(controller_->IsShowing());

  // Both windows are restored.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsNormalStateType());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsNormalStateType());
}

}  // namespace ash
