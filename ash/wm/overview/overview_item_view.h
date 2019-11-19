// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_

#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/window_mini_view.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class ImageButton;
class View;
}  // namespace views

namespace ash {
class OverviewItem;

// OverviewItemView covers the overview window and listens for events.
class ASH_EXPORT OverviewItemView
    : public WindowMiniView,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  // The visibility of the header. It may be fully visible or invisible, or
  // everything but the close button is visible.
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  // If |show_preview| is true, this class will contain a child view which
  // mirrors |window|.
  OverviewItemView(OverviewItem* overview_item,
                   aura::Window* window,
                   bool show_preview);
  ~OverviewItemView() override;

  // Fades the app icon and title out if |visibility| is kInvisible, in
  // otherwise. If |close_button_| is not null, also fades the close button in
  // if |visibility| is kVisible, out otherwise. Sets
  // |current_header_visibility_| to |visibility|.
  void SetHeaderVisibility(HeaderVisibility visibility);

  // Hides the close button instantaneously, and then fades it in slowly and
  // with a long delay. Sets |current_header_visibility_| to kVisible. Assumes
  // that |close_button_| is not null, and that |current_header_visibility_| is
  // not kInvisible.
  void HideCloseInstantlyAndThenShowItSlowly();

  // Called when |overview_item_| is about to be restored to its original state
  // outside of overview.
  void OnOverviewItemWindowRestoring();

  // Refreshes |preview_view_| so that its content is up-to-date. Used by tab
  // dragging.
  void RefreshPreviewView();

  // Sets or hides rounded corners on |preview_view_|, if it exists.
  void UpdatePreviewRoundedCorners(bool show, float rounding);

  // WindowMiniView:
  int GetMargin() const override;
  gfx::Rect GetHeaderBounds() const override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  gfx::Rect GetHighlightBoundsInScreen() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  gfx::Point GetMagnifierFocusPointInScreen() override;

  views::ImageButton* close_button() { return close_button_; }

 protected:
  // views::View:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // The OverviewItem which owns the widget which houses this view. Non-null
  // until |OnOverviewItemWindowRestoring| is called.
  OverviewItem* overview_item_;

  views::ImageButton* close_button_;

  HeaderVisibility current_header_visibility_ = HeaderVisibility::kVisible;

  DISALLOW_COPY_AND_ASSIGN(OverviewItemView);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
