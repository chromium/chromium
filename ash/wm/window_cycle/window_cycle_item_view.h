// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/scoped_layer_tree_synchronizer.h"
#include "ash/wm/window_mini_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class MouseEvent;
}  // namespace ui

namespace ash {

class SnapGroup;
class WindowCycleController;

// This view represents a single aura::Window by displaying a title and a
// thumbnail of the window's contents.
class ASH_EXPORT WindowCycleItemView : public WindowMiniView {
  METADATA_HEADER(WindowCycleItemView, WindowMiniView)

 public:
  explicit WindowCycleItemView(aura::Window* window);
  WindowCycleItemView(const WindowCycleItemView&) = delete;
  WindowCycleItemView& operator=(const WindowCycleItemView&) = delete;
  ~WindowCycleItemView() override;

  // All previews are the same height (this is achieved via a combination of
  // scaling and padding).
  static constexpr int kFixedPreviewHeightDp = 256;

  // WindowMiniView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size GetPreviewViewSize() const override;
  void Layout(PassKey) override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // WindowMiniViewBase:
  void RefreshItemVisuals() override;

 private:
  std::unique_ptr<ScopedLayerTreeSynchronizer> layer_tree_synchronizer_;
  const raw_ptr<WindowCycleController> window_cycle_controller_;
};

// Container view used to host multiple `WindowCycleItemView`s and be the focus
// target for window groups while tabbing in window cycle view.
class ASH_EXPORT GroupContainerCycleView : public WindowMiniViewBase {
  METADATA_HEADER(GroupContainerCycleView, WindowMiniViewBase)

 public:
  explicit GroupContainerCycleView(SnapGroup* snap_group);
  GroupContainerCycleView(const GroupContainerCycleView&) = delete;
  GroupContainerCycleView& operator=(const GroupContainerCycleView&) = delete;
  ~GroupContainerCycleView() override;

  const std::vector<raw_ptr<WindowCycleItemView, VectorExperimental>>&
  mini_views() const {
    return mini_views_;
  }

  // WindowMiniViewBase:
  bool Contains(aura::Window* window) const override;
  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point) const override;
  void SetShowPreview(bool show) override;
  void RefreshItemVisuals() override;
  int TryRemovingChildItem(aura::Window* destroying_window) override;
  gfx::RoundedCornersF GetRoundedCorners() const override;
  void SetSelectedWindowForFocus(aura::Window* window) override;
  void ClearFocusSelection() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  std::vector<raw_ptr<WindowCycleItemView, VectorExperimental>> mini_views_;

  // True if the `SnapGroup` represented by `this` has horizontal window layout,
  // false otherwise.
  bool is_layout_horizontal_ = false;

  // True if `this` is the first time a focus selection request is made to this
  // item.
  bool is_first_focus_selection_request_ = true;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
