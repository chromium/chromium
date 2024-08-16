// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller_impl.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

using ArcOverlayControllerImplTest = AshTestBase;

TEST_F(ArcOverlayControllerImplTest, OverlayComesOnTop) {
  auto* host_widget = TestWidgetBuilder().BuildOwnedByNativeWidget();
  auto* host_window = host_widget->GetNativeWindow();

  aura::Window child_window(nullptr);
  child_window.Init(ui::LAYER_NOT_DRAWN);
  host_window->AddChild(&child_window);

  auto* overlay_widget = TestWidgetBuilder().BuildOwnedByNativeWidget();

  // This sets the host_window as `ArcOverlayControllerImpl::host_window_`,
  // which the arc overlay should be hosted by.
  ArcOverlayControllerImpl controller(host_window);

  controller.AttachOverlay(overlay_widget->GetNativeWindow());

  // Make sure that the overlay is at the top of the stack i.e. the last child
  // of `host_window`. Note that an extra window `NativeViewHostAuraClip` is
  // added as parent of the overlay so we compare with the parent.
  EXPECT_EQ(host_window->children().back(),
            overlay_widget->GetNativeWindow()->parent());
}

TEST_F(ArcOverlayControllerImplTest,
       OverlayNativeViewHostAccessibleProperties) {
  ArcOverlayControllerImpl controller(
      TestWidgetBuilder().BuildOwnedByNativeWidget()->GetNativeWindow());
  ui::AXNodeData data;

  controller.overlay_container_for_test()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kApplication);
  EXPECT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(data.GetNameFrom(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

}  // namespace
}  // namespace ash
