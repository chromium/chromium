// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_weather_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

AmbientWeatherModel* GetWeatherModel() {
  return Shell::Get()->ambient_controller()->GetAmbientWeatherModel();
}

// Use a "no session" test so the glanceables widget is not automatically
// created at the start of the test.
// TODO(crbug.com/1353119): Once glanceables are shown by code in the
// chrome/browser/ash layer, switch this to AshTestBase.
class GlanceablesWeatherViewTest : public NoSessionAshTestBase {
 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesWeatherViewTest, Basics) {
  GlanceablesController* controller = Shell::Get()->glanceables_controller();
  ASSERT_TRUE(controller);
  controller->CreateUi();

  GlanceablesWeatherView* view =
      controller->view_for_test()->weather_view_for_test();
  ASSERT_TRUE(view);

  // Icon starts blank.
  views::ImageView* icon = view->icon_for_test();
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
  EXPECT_EQ(u"72° F", view->temperature_for_test()->GetText());
}

}  // namespace
}  // namespace ash
