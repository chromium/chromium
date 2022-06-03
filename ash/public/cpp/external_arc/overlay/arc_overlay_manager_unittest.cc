// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"

#include "ash/test/test_widget_builder.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

class ArcOverlayManagerTest : public exo::test::ExoTestBase {};

TEST_F(ArcOverlayManagerTest, SkipImeProcessingProperty) {
  ArcOverlayManager manager;

  const std::string token = "billing_id:overlay_token";
  auto* widget = TestWidgetBuilder().BuildOwnedByNativeWidget();

  exo::test::ShellSurfaceBuilder builder(gfx::Size(100, 100));
  auto shell_surface = builder.BuildShellSurface();
  auto* shell_surface_window = shell_surface->GetWidget()->GetNativeWindow();
  shell_surface->root_surface()->SetClientSurfaceId(token.c_str());
  shell_surface_window->SetProperty(aura::client::kAppType,
                                    static_cast<int>(ash::AppType::ARC_APP));
  manager.OnWindowInitialized(shell_surface_window);
  auto deregister_closure =
      manager.RegisterHostWindow("overlay_token", widget->GetNativeWindow());

  EXPECT_FALSE(
      shell_surface_window->GetProperty(aura::client::kSkipImeProcessing));
  manager.OnWindowVisibilityChanged(shell_surface_window, true);
  EXPECT_TRUE(
      shell_surface_window->GetProperty(aura::client::kSkipImeProcessing));
}

}  // namespace ash
