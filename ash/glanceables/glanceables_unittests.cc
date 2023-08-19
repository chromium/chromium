// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/glanceables/test_glanceables_delegate.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

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

 protected:
  raw_ptr<GlanceablesController, DanglingUntriaged | ExperimentalAsh>
      controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesTest, CreateAndDestroyUi) {
  ASSERT_EQ(0, GetTestDelegate()->on_glanceables_closed_count());

  controller_->CreateUi();

  // A fullscreen widget was created.
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsFullscreen());

  // The controller's view is the widget's contents view.
  views::View* view = GetGlanceablesView();
  EXPECT_TRUE(view);
  EXPECT_EQ(view, widget->GetContentsView());

  // Backdrop was applied.
  EXPECT_GT(GetWidget()->GetLayer()->background_blur(), 0);
  EXPECT_TRUE(view->GetBackground());

  controller_->DestroyUi();

  // Widget and glanceables view are destroyed.
  EXPECT_FALSE(GetWidget());
  EXPECT_FALSE(GetGlanceablesView());

  // Delegate was notified that glanceables were closed.
  EXPECT_EQ(1, GetTestDelegate()->on_glanceables_closed_count());
}

TEST_F(GlanceablesTest, HidesInTabletMode) {
  controller_->CreateUi();
  ASSERT_TRUE(controller_->IsShowing());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(controller_->IsShowing());
}

TEST_F(GlanceablesTest, GlanceablesViewCreatesChildViews) {
  controller_->CreateUi();

  GlanceablesView* view = GetGlanceablesView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(GetWelcomeLabel());
  EXPECT_TRUE(GetWeatherIcon());
  EXPECT_TRUE(GetWeatherTemperature());
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

}  // namespace ash
