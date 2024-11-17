// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GROUP_ITEM_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GROUP_ITEM_H_

#include <memory>

#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace ash {

class OverviewSession;

// This class implements `OverviewItemBase` and represents a window group in
// overview mode. It is the composite item of the overview item hierarchy that
// contains two individual `OverviewItem`s. It is responsible to place the group
// item in the correct bounds calculated by `OverviewGrid`. It will also be the
// target when handling overview group item drag events.
class OverviewGroupItem : public OverviewItemBase,
                          public OverviewItem::WindowDestructionDelegate {
 public:
  using Windows = aura::Window::Windows;

  OverviewGroupItem(const Windows& windows,
                    OverviewSession* overview_session,
                    OverviewGrid* overview_grid);
  OverviewGroupItem(const OverviewGroupItem&) = delete;
  OverviewGroupItem& operator=(const OverviewGroupItem&) = delete;
  ~OverviewGroupItem() override;

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

  // OverviewItem::WindowDestructionDelegate:
  void OnOverviewItemWindowDestroying(OverviewItem* overview_item,
                                      bool reposition) override;

  const std::vector<std::unique_ptr<OverviewItem>>& overview_items_for_testing()
      const {
    return overview_items_;
  }

 protected:
  // OverviewItemBase:
  void HandleDragEvent(const gfx::PointF& location_in_screen) override;

 private:
  // Creates `item_widget_` with `OverviewGroupContainerView` as its contents
  // view.
  void CreateItemWidget();

  // A list of `OverviewItem`s hosted and owned by `this`.
  std::vector<std::unique_ptr<OverviewItem>> overview_items_;

  // The contents view of the `item_widget_`.
  raw_ptr<views::View> overview_group_container_view_ = nullptr;

  base::WeakPtrFactory<OverviewGroupItem> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GROUP_ITEM_H_
