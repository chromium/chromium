// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

// Use a "no session" test so the glanceables widget is not automatically
// created at the start of the test.
// TODO(crbug.com/1353119): Once glanceables are shown by code in the
// chrome/browser/ash layer, switch this to AshTestBase.
class GlanceablesControllerTest : public NoSessionAshTestBase {
 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesControllerTest, CreateUi) {
  GlanceablesController* controller = Shell::Get()->glanceables_controller();
  ASSERT_TRUE(controller);

  controller->CreateUi();

  // A fullscreen widget was created.
  views::Widget* widget = controller->widget_for_test();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsFullscreen());

  // The controller's view is the widget's contents view.
  views::View* view = controller->view_for_test();
  EXPECT_TRUE(view);
  EXPECT_EQ(view, widget->GetContentsView());
}

TEST_F(GlanceablesControllerTest, DestroyUi) {
  auto* controller = Shell::Get()->glanceables_controller();
  ASSERT_TRUE(controller);

  controller->CreateUi();
  controller->DestroyUi();

  EXPECT_FALSE(controller->widget_for_test());
  EXPECT_FALSE(controller->view_for_test());
}

}  // namespace
}  // namespace ash
