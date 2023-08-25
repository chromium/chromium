// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mini_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

class SnapGroup;

// This view represents a single aura::Window by displaying a title and a
// thumbnail of the window's contents.
class ASH_EXPORT WindowCycleItemView : public WindowMiniView {
 public:
  METADATA_HEADER(WindowCycleItemView);

  explicit WindowCycleItemView(aura::Window* window);
  WindowCycleItemView(const WindowCycleItemView&) = delete;
  WindowCycleItemView& operator=(const WindowCycleItemView&) = delete;
  ~WindowCycleItemView() override = default;

  // All previews are the same height (this is achieved via a combination of
  // scaling and padding).
  static constexpr int kFixedPreviewHeightDp = 256;

  // WindowMiniView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size GetPreviewViewSize() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // WindowMiniViewBase:
  void RefreshItemVisuals() override;
};

// Container view used to host multiple `WindowCycleItemView`s and be the focus
// target for window groups while tabbing in window cycle view.
class GroupContainerCycleView : public WindowMiniViewBase {
 public:
  METADATA_HEADER(GroupContainerCycleView);

  explicit GroupContainerCycleView(SnapGroup* snap_group);
  GroupContainerCycleView(const GroupContainerCycleView&) = delete;
  GroupContainerCycleView& operator=(const GroupContainerCycleView&) = delete;
  ~GroupContainerCycleView() override;

  // WindowMiniViewBase:
  bool Contains(aura::Window* window) const override;
  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point) const override;
  void RefreshItemVisuals() override;
  int TryRemovingChildItem(aura::Window* destroying_window) override;
  gfx::RoundedCornersF GetRoundedCorners() const override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // TODO(b/297070130): Use vector store the child `WindowCycleItemView` hosted
  // by this.
  raw_ptr<WindowCycleItemView> mini_view1_;
  raw_ptr<WindowCycleItemView> mini_view2_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
