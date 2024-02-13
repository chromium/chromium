// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_

#include <string>

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class AssistantCardElement;
class AssistantViewDelegate;

// AssistantCardElementView is the visual representation of an
// AssistantCardElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantCardElementView
    : public AssistantUiElementView,
      public AshWebView::Observer {
  METADATA_HEADER(AssistantCardElementView, AssistantUiElementView)

 public:
  AssistantCardElementView(AssistantViewDelegate* delegate,
                           const AssistantCardElement* card_element);

  AssistantCardElementView(const AssistantCardElementView&) = delete;
  AssistantCardElementView& operator=(const AssistantCardElementView&) = delete;

  ~AssistantCardElementView() override;

  // AssistantUiElementView:
  ui::Layer* GetLayerForAnimating() override;
  std::string ToStringForTesting() const override;
  void AddedToWidget() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;
  std::unique_ptr<ElementAnimator> CreateAnimator() override;

  // AshWebView::Observer:
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;
  void DidChangeFocusedNode(const gfx::Rect& node_bounds_in_screen) override;

  // Returns a reference to the native view associated with the underlying web
  // contents. When animating AssistantCardElementView, we should animate the
  // layer for the native view as opposed to painting to and animating a layer
  // belonging to AssistantCardElementView.
  gfx::NativeView native_view() { return contents_view_->GetNativeView(); }

 private:
  void InitLayout();

  raw_ptr<AshWebView> contents_view_ = nullptr;

  const raw_ptr<AssistantViewDelegate> delegate_;
  const raw_ptr<const AssistantCardElement> card_element_;

  // Rect of the focused node in the |contents_view_|.
  gfx::Rect focused_node_rect_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_CARD_ELEMENT_VIEW_H_
