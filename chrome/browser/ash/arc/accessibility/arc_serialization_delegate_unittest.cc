// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_serialization_delegate.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/accessibility/arc_serialization_delegate.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/test/android_accessibility_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
class TestAccessibilityInfoDataWrapper
    : public ax::android::AccessibilityInfoDataWrapper {
 public:
  explicit TestAccessibilityInfoDataWrapper(
      ax::android::AXTreeSourceAndroid* tree_source_)
      : AccessibilityInfoDataWrapper(tree_source_) {}

  // AccessibilityInfoDataWrapper overrides:
  bool IsNode() const override { return false; }
  ax::android::mojom::AccessibilityNodeInfoData* GetNode() const override {
    return nullptr;
  }
  ax::android::mojom::AccessibilityWindowInfoData* GetWindow() const override {
    return nullptr;
  }
  int32_t GetId() const override { return id_; }
  const gfx::Rect GetBounds() const override { return bounds_; }
  bool IsVisibleToUser() const override { return true; }
  bool IsWebNode() const override { return false; }
  bool IsIgnored() const override { return false; }
  bool IsImportantInAndroid() const override { return true; }
  bool IsFocusableInFullFocusMode() const override { return true; }
  bool IsAccessibilityFocusableContainer() const override { return true; }
  void PopulateAXRole(ui::AXNodeData* out_data) const override {}
  void PopulateAXState(ui::AXNodeData* out_data) const override {}
  std::string ComputeAXName(bool do_recursive) const override { return ""; }
  void GetChildren(
      std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>*
          children) const override {}
  int32_t GetWindowId() const override { return 1; }

  int32_t id_ = 1;
  gfx::Rect bounds_;
};

class TestTreeSource : public ax::android::AXTreeSourceAndroid {
 public:
  explicit TestTreeSource(aura::Window* window)
      : AXTreeSourceAndroid(nullptr,
                            std::make_unique<ArcSerializationDelegate>(),
                            window) {}
  ax::android::AccessibilityInfoDataWrapper* GetRoot() const override {
    return root_;
  }
  raw_ptr<ax::android::AccessibilityInfoDataWrapper> root_;
};

class ArcSerializationDelegateTest : public ash::AshTestBase {
 public:
  ArcSerializationDelegateTest() = default;

  std::unique_ptr<exo::WMHelper> wm_helper = std::make_unique<exo::WMHelper>();
};

TEST_F(ArcSerializationDelegateTest, NonRootNodeBounds) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper root(&tree_source);
  root.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &root;

  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.id_ = 2;
  data.bounds_ = gfx::Rect(20, 20, 30, 40);

  ui::AXNodeData out_data;
  tree_source.serialization_delegate().PopulateBounds(data, out_data);

  EXPECT_EQ(gfx::RectF(10, 10, 30, 40), out_data.relative_bounds.bounds);
}

TEST_F(ArcSerializationDelegateTest, RootNodeBounds) {
  UpdateDisplay("400x300");
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  tree_source.serialization_delegate().PopulateBounds(data, out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(ArcSerializationDelegateTest, RootNodeBoundsOnExternalDisplay) {
  UpdateDisplay("400x300,600x500");

  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(410, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  tree_source.serialization_delegate().PopulateBounds(data, out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(ArcSerializationDelegateTest, BoundsScalingPiArc) {
  UpdateDisplay("400x300*2");  // 2x device scale factor.

  // With default_scale_cancellation, Android has default (1x) scale factor.
  wm_helper->SetDefaultScaleCancellation(true);
  auto shell_surface = exo::test::ShellSurfaceBuilder()
                           .SetGeometry(gfx::Rect(10, 10, 100, 100))
                           .EnableDefaultScaleCancellation()
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 100, 100);  // PX in Android.
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  tree_source.serialization_delegate().PopulateBounds(data, out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(ArcSerializationDelegateTest, BoundsScalingFromRvcArcAndLater) {
  UpdateDisplay("400x300*2");  // 2x device scale factor.

  // Without default_scale_cancellation, Android use the same (2x) scale factor.
  wm_helper->SetDefaultScaleCancellation(false);
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(20, 20, 200, 200);  // PX in Android.
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  tree_source.serialization_delegate().PopulateBounds(data, out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

}  // namespace arc
