// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_DROP_TARGET_H_
#define ASH_WM_OVERVIEW_OVERVIEW_DROP_TARGET_H_

#include "ash/wm/overview/overview_item_base.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class OverviewGrid;

// `OverviewDropTarget` is an indicator for where a window will end up when
// released during an overview drag.
class OverviewDropTarget : public OverviewItemBase {
 public:
  explicit OverviewDropTarget(OverviewGrid* overview_grid);
  OverviewDropTarget(const OverviewDropTarget&) = delete;
  OverviewDropTarget& operator=(const OverviewDropTarget&) = delete;
  ~OverviewDropTarget() override;

  // Changes the color of the drop target depending on whether
  // `location_in_screen` intersects it.
  void UpdateBackgroundVisibility(const gfx::Point& location_in_screen);

  // OverviewItemBase:
  void SetOpacity(float opacity) override;
  aura::Window::Windows GetWindowsForHomeGesture() override;
  void HideForSavedDeskLibrary(bool animate) override;
  void RevertHideForSavedDeskLibrary(bool animate) override;
  void UpdateMirrorsForDragging(bool is_touch_dragging) override;
  void DestroyMirrorsForDragging() override;
  aura::Window* GetWindow() override;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> GetWindows() override;
  bool HasVisibleOnAllDesksWindow() override;
  bool Contains(const aura::Window* target) const override;
  OverviewItem* GetLeafItemForWindow(aura::Window* window) override;
  void RestoreWindow(bool reset_transform, bool animate) override;
  void SetBounds(const gfx::RectF& target_bounds,
                 OverviewAnimationType animation_type) override;
  gfx::Transform ComputeTargetTransform(
      const gfx::RectF& target_bounds) override;
  gfx::RectF GetWindowsUnionScreenBounds() const override;
  gfx::RectF GetTargetBoundsWithInsets() const override;
  gfx::RectF GetTransformedBounds() const override;
  float GetItemScale(int height) override;
  void ScaleUpSelectedItem(OverviewAnimationType animation_type) override;
  void EnsureVisible() override;
  std::vector<views::Widget*> GetFocusableWidgets() override;
  views::View* GetBackDropView() const override;
  bool ShouldHaveShadow() const override;
  void UpdateRoundedCornersAndShadow() override;
  float GetOpacity() const override;
  void PrepareForOverview() override;
  void SetShouldUseSpawnAnimation(bool value) override;
  void OnStartingAnimationComplete() override;
  void Restack() override;
  void StartDrag() override;
  void OnOverviewItemDragStarted() override;
  void OnOverviewItemDragEnded(bool snap) override;
  void OnOverviewItemContinuousScroll(const gfx::Transform& target_transform,
                                      float scroll_ratio) override;
  void UpdateCannotSnapWarningVisibility(bool animate) override;
  void HideCannotSnapWarning(bool animate) override;
  void OnMovingItemToAnotherDesk() override;
  void Shutdown() override;
  void AnimateAndCloseItem(bool up) override;
  void StopWidgetAnimation() override;
  OverviewItemFillMode GetOverviewItemFillMode() const override;
  void UpdateOverviewItemFillMode() override;
  const gfx::RoundedCornersF GetRoundedCorners() const override;

 private:
  void CreateItemWidget();
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_DROP_TARGET_H_
