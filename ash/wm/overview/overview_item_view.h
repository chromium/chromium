// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_

#include "ash/wm/overview/event_handler_delegate.h"
#include "ash/wm/overview/overview_focusable_view.h"
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
                                    public OverviewFocusableView {
 public:
  METADATA_HEADER(OverviewItemView);

  // If `show_preview` is true, this class will contain a child view which
  // mirrors `window`.
  OverviewItemView(OverviewItem* overview_item,
                   EventHandlerDelegate* event_handler_delegate,
                   views::Button::PressedCallback close_callback,
                   aura::Window* window,
                   bool show_preview);
  OverviewItemView(const OverviewItemView&) = delete;
  OverviewItemView& operator=(const OverviewItemView&) = delete;
  ~OverviewItemView() override;

  CloseButton* close_button() const { return close_button_; }

  void SetCloseButtonVisible(bool visible);

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
  gfx::Size GetPreviewViewSize() const override;

  // WindowMiniViewBase:
  void RefreshItemVisuals() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  OverviewItemBase* GetOverviewItem() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  bool MaybeActivateFocusedViewOnOverviewExit(
      OverviewSession* overview_session) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;
  gfx::Point GetMagnifierFocusPointInScreen() override;

 protected:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 private:
  // The `OverviewItem` whose item widget owns and hosts this view. Please note
  // that `item_widget_` may outlive its corresponding `OverviewItem` which will
  // make `overview_item_` null while `this` is still alive. `overview_item_`
  // will be explicitly set to null when `OnOverviewItemWindowRestoring()` is
  // called.
  raw_ptr<OverviewItem> overview_item_;

  // Points to the event handling delegate to handle the events forwarded from
  // `this`.
  raw_ptr<EventHandlerDelegate> event_handler_delegate_;

  raw_ptr<CloseButton, ExperimentalAsh> close_button_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
