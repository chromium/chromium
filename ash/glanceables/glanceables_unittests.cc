// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_restore_view.h"
#include "ash/glanceables/glanceables_up_next_view.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/glanceables/test_glanceables_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
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

}  // namespace ash
