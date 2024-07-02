// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantUiElementViewFactory;
class AssistantViewDelegate;

// UiElementContainerView is the child of AssistantMainView concerned with
// laying out Assistant UI element views in response to Assistant interaction
// model events.
class COMPONENT_EXPORT(ASSISTANT_UI) UiElementContainerView
    : public AnimatedContainerView {
  METADATA_HEADER(UiElementContainerView, AnimatedContainerView)

 public:
  explicit UiElementContainerView(AssistantViewDelegate* delegate);

  UiElementContainerView(const UiElementContainerView&) = delete;
  UiElementContainerView& operator=(const UiElementContainerView&) = delete;

  ~UiElementContainerView() override;

  void OnOverflowIndicatorVisibilityChanged(bool is_visible);

  // AnimatedContainerView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  void OnCommittedQueryChanged(const AssistantQuery& query) override;

  // views::View:
  void OnThemeChanged() override;

  // AssistantScrollView::Observer:
  void OnContentsPreferredSizeChanged(views::View* content_view) override;

 private:
  void InitLayout();

  SkColor GetOverflowIndicatorBackgroundColor() const;

  // AnimatedContainerView:
  std::unique_ptr<ElementAnimator> HandleUiElement(
      const AssistantUiElement* ui_element) override;
  void OnAllViewsAnimatedIn() override;

  raw_ptr<views::View> scroll_indicator_ = nullptr;  // Owned by view hierarchy.

  // Factory instance used to construct views for modeled UI elements.
  std::unique_ptr<AssistantUiElementViewFactory> view_factory_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
