// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_

#include "ash/wm/overview/overview_highlightable_view.h"
#include "ash/wm/window_mini_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace ash {

class CloseButton;
class OverviewItem;

// OverviewItemView covers the overview window, provides an overview only header
// and handles events. It hosts a mirror view if the window is minimized.
class ASH_EXPORT OverviewItemView : public WindowMiniView,
                                    public OverviewHighlightableView {
 public:
  METADATA_HEADER(OverviewItemView);

  // The visibility of the header. It may be fully visible or invisible, or
  // everything but the close button is visible.
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  // If `show_preview` is true, this class will contain a child view which
  // mirrors `window`.
  OverviewItemView(OverviewItem* overview_item,
                   views::Button::PressedCallback close_callback,
                   aura::Window* window,
                   bool show_preview);
  OverviewItemView(const OverviewItemView&) = delete;
  OverviewItemView& operator=(const OverviewItemView&) = delete;
  ~OverviewItemView() override;

  // Fades the app icon and title out if `visibility` is kInvisible, in
  // otherwise. If `close_button_` is not null, also fades the close button in
  // if `visibility` is kVisible, out otherwise. Sets
  // `current_header_visibility_` to `visibility`. Fades in if `animate` is
  // true, otherwise shows immediately.
  void SetHeaderVisibility(HeaderVisibility visibility, bool animate);

  // Hides the close button instantaneously, and then fades it in slowly and
  // with a long delay. Sets `current_header_visibility_` to kVisible. Assumes
  // that `close_button_` is not null, and that `current_header_visibility_` is
  // not kInvisible.
  void HideCloseInstantlyAndThenShowItSlowly();

  // Called when `overview_item_` is about to be restored to its original state
  // outside of overview.
  void OnOverviewItemWindowRestoring();

  // Refreshes `preview_view_` so that its content is up-to-date. Used by tab
  // dragging.
  void RefreshPreviewView();

  // WindowMiniView:
  gfx::Rect GetHeaderBounds() const override;
  gfx::Size GetPreviewViewSize() const override;

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView(bool primary_action) override;
  void MaybeSwapHighlightedView(bool right) override;
  bool MaybeActivateHighlightedViewOnOverviewExit(
      OverviewSession* overview_session) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;
  gfx::Point GetMagnifierFocusPointInScreen() override;

  CloseButton* close_button() const { return close_button_; }

 protected:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 private:
  // The OverviewItem which owns the widget which houses this view. Non-null
  // until `OnOverviewItemWindowRestoring` is called.
  raw_ptr<OverviewItem, ExperimentalAsh> overview_item_;

  raw_ptr<CloseButton, ExperimentalAsh> close_button_;

  HeaderVisibility current_header_visibility_ = HeaderVisibility::kVisible;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
