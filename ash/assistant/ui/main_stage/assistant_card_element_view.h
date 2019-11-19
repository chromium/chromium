// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "ui/views/view.h"

namespace ash {

class AssistantCardElement;
class AssistantViewDelegate;

// AssistantCardElementView is the visual representation of an
// AssistantCardElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantCardElementView
    : public views::View,
      public content::NavigableContentsObserver {
 public:
  AssistantCardElementView(AssistantViewDelegate* delegate,
                           const AssistantCardElement* card_element);
  ~AssistantCardElementView() override;

  // views::View:
  const char* GetClassName() const override;
  void AddedToWidget() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void OnFocus() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;

  // content::NavigableContentsObserver:
  void DidAutoResizeView(const gfx::Size& new_size) override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;

  // Returns a reference to the native view associated with the underlying web
  // contents. When animating AssistantCardElementView, we should animate the
  // layer for the native view as opposed to painting to and animating a layer
  // belonging to AssistantCardElementView.
  gfx::NativeView native_view() { return contents_->GetView()->native_view(); }

 private:
  void InitLayout(const AssistantCardElement* card_element);

  AssistantViewDelegate* const delegate_;

  // Owned by AssistantCardElement.
  content::NavigableContents* const contents_;

  // Rect of the focused node in the |contents_|.
  gfx::Rect focused_node_rect_;

  DISALLOW_COPY_AND_ASSIGN(AssistantCardElementView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_
