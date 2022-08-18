// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_view.h"

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

// Use a "no session" test so the glanceables widget is not automatically
// created at the start of the test.
// TODO(crbug.com/1353119): Once glanceables are shown by code in the
// chrome/browser/ash layer, switch this to AshTestBase.
class GlanceablesViewTest : public NoSessionAshTestBase {
 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesViewTest, Basics) {
  GlanceablesController* controller = Shell::Get()->glanceables_controller();
  ASSERT_TRUE(controller);
  controller->CreateUi();

  GlanceablesView* view = controller->view_for_test();
  ASSERT_TRUE(view);

  // Welcome label was created.
  GlanceablesWelcomeLabel* welcome_label = view->welcome_label_for_test();
  ASSERT_TRUE(welcome_label);
  EXPECT_FALSE(welcome_label->GetText().empty());
}

}  // namespace
}  // namespace ash
