// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_

#include "ash/wm/overview/event_handler_delegate.h"
#include "ash/wm/window_mini_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class CloseButton;
class OverviewItem;
class OverviewSession;

// OverviewItemView covers the overview window, provides an overview only header
// and handles events. It hosts a mirror view if the window is minimized.
class ASH_EXPORT OverviewItemView : public WindowMiniView {
  METADATA_HEADER(OverviewItemView, WindowMiniView)

 public:
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

  OverviewItem* overview_item() { return overview_item_; }
  CloseButton* close_button() { return close_button_; }

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

  // Called when the user exits overview by using 3-finger vertical trackpad
  // swipes.
  void AcceptSelection(OverviewSession* overview_session);

  // WindowMiniView:
  gfx::Size GetPreviewViewSize() const override;
  void RefreshItemVisuals() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void UpdateAccessibleDescription();

  // The `OverviewItem` whose item widget owns and hosts this view. Please note
  // that `item_widget_` may outlive its corresponding `OverviewItem` which will
  // make `overview_item_` null while `this` is still alive. `overview_item_`
  // will be explicitly set to null when `OnOverviewItemWindowRestoring()` is
  // called.
  raw_ptr<OverviewItem> overview_item_;

  // Points to the event handling delegate to handle the events forwarded from
  // `this`.
  raw_ptr<EventHandlerDelegate> event_handler_delegate_;

  raw_ptr<CloseButton> close_button_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
