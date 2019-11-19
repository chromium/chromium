// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantResponse;
class AssistantCardElement;
class AssistantTextElement;
class AssistantViewDelegate;

// UiElementContainerView is the child of AssistantMainView concerned with
// laying out text views and embedded card views in response to Assistant
// interaction model UI element events.
class COMPONENT_EXPORT(ASSISTANT_UI) UiElementContainerView
    : public AnimatedContainerView {
 public:
  explicit UiElementContainerView(AssistantViewDelegate* delegate);
  ~UiElementContainerView() override;

  // AnimatedContainerView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  gfx::Size GetMinimumSize() const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;
  void OnCommittedQueryChanged(const AssistantQuery& query) override;

 private:
  void InitLayout();

  // AnimatedContainerView:
  void HandleResponse(const AssistantResponse& response) override;
  void OnAllViewsRemoved() override;
  void OnAllViewsAnimatedIn() override;

  void OnCardElementAdded(const AssistantCardElement* card_element);
  void OnTextElementAdded(const AssistantTextElement* text_element);

  // Whether or not the card we are adding is the first card for the current
  // Assistant response. The first card requires the addition of a top margin.
  bool is_first_card_ = true;

  DISALLOW_COPY_AND_ASSIGN(UiElementContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
