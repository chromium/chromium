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
  aura::Window* GetWindow() override;
  std::vector<aura::Window*> GetWindows() override;
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
  OverviewFocusableView* GetFocusableView() const override;
  views::View* GetBackDropView() const override;
  void UpdateRoundedCornersAndShadow() override;
  void SetOpacity(float opacity) override;
  float GetOpacity() const override;
  void PrepareForOverview() override;
  void OnStartingAnimationComplete() override;
  void HideForSavedDeskLibrary(bool animate) override;
  void RevertHideForSavedDeskLibrary(bool animate) override;
  void CloseWindows() override;
  void Restack() override;
  void StartDrag() override;
  void OnOverviewItemDragStarted(OverviewItemBase* item) override;
  void OnOverviewItemDragEnded(bool snap) override;
  void OnOverviewItemContinuousScroll(const gfx::Transform& target_transform,
                                      float scroll_ratio) override;
  void SetVisibleDuringItemDragging(bool visible, bool animate) override;
  void UpdateCannotSnapWarningVisibility(bool animate) override;
  void HideCannotSnapWarning(bool animate) override;
  void OnMovingItemToAnotherDesk() override;
  void UpdateMirrorsForDragging(bool is_touch_dragging) override;
  void DestroyMirrorsForDragging() override;
  void Shutdown() override;
  void AnimateAndCloseItem(bool up) override;
  void StopWidgetAnimation() override;
  OverviewGridWindowFillMode GetWindowDimensionsType() const override;
  void UpdateWindowDimensionsType() override;
  gfx::Point GetMagnifierFocusPointInScreen() const override;
  const gfx::RoundedCornersF GetRoundedCorners() const override;

 protected:
  // OverviewItemBase:
  void CreateItemWidget() override;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_DROP_TARGET_H_
