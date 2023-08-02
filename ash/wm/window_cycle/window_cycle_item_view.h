// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mini_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}

namespace ash {

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

  // Shows the preview and icon. For performance reasons, these are not created
  // on construction. This should be called at most one time during the lifetime
  // of |this|.
  void ShowPreview();

  // WindowMiniView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size GetPreviewViewSize() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
};

// Container view used to host multiple `WindowCycleItemView`s and be the focus
// target for window groups while tabbing in window cycle view.
class GroupContainerView : public FocusableView {
 public:
  METADATA_HEADER(GroupContainerView);

  GroupContainerView();
  GroupContainerView(const GroupContainerView&) = delete;
  GroupContainerView& operator=(const GroupContainerView&) = delete;
  ~GroupContainerView() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_ITEM_VIEW_H_
