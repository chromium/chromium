// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash {

using WmShadowControllerDelegateTest = AshTestBase;

TEST_F(WmShadowControllerDelegateTest,
       UpdateShadowRoundedCornersEnterExitOverview) {
  auto window = CreateTestWindow();

  // Manually set a corner radius to window and update the shadow.
  window->SetProperty(aura::client::kWindowCornerRadiusKey, 12);
  auto* shadow_controller = Shell::Get()->shadow_controller();
  shadow_controller->UpdateShadowForWindow(window.get());

  // Before entering Overview, the shadow should have a same rounded corner
  // radius with its window.
  auto* shadow = shadow_controller->GetShadowForWindow(window.get());
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 12);

  // Enter Overview, the shadow's rounded corner radius becomes 0.
  ToggleOverview();
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 0);

  // Exit Overview, the shadow's rounded corner radius is reset to window
  // rounded corner radius.
  ToggleOverview();
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 12);
}

}  // namespace ash
