// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/assistant_response_container_view.h"

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_text_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_error_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredWidthDip = 600;

}  // namespace

AssistantResponseContainerView::AssistantResponseContainerView(
    AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
  SetID(AmbientViewID::kAmbientAssistantResponseContainerView);
  InitLayout();
}

AssistantResponseContainerView::~AssistantResponseContainerView() = default;

const char* AssistantResponseContainerView::GetClassName() const {
  return "AssistantResponseContainerView";
}

gfx::Size AssistantResponseContainerView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip,
                   content_view()->GetHeightForWidth(kPreferredWidthDip));
}

void AssistantResponseContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  content_view->SetSize(CalculatePreferredSize());
}

void AssistantResponseContainerView::InitLayout() {
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

std::unique_ptr<ElementAnimator>
AssistantResponseContainerView::HandleUiElement(
    const AssistantUiElement* ui_element) {
  switch (ui_element->type()) {
    case AssistantUiElementType::kCard:
      // For card elements, we instead use the "fallback" message for HTML
      // card rendering as the text response.
      AddTextElementView(new AssistantTextElement(
          static_cast<const AssistantCardElement*>(ui_element)->fallback()));
      break;
    case AssistantUiElementType::kError:
      AddErrorElementView(
          static_cast<const AssistantErrorElement*>(ui_element));
      break;
    case AssistantUiElementType::kText:
      AddTextElementView(static_cast<const AssistantTextElement*>(ui_element));
      break;
  }

  // Return |nullptr| to prevent animations.
  return nullptr;
}

void AssistantResponseContainerView::AddTextElementView(
    const AssistantTextElement* text_element) {
  content_view()->AddChildView(
      std::make_unique<AssistantTextElementView>(text_element));
}

void AssistantResponseContainerView::AddErrorElementView(
    const AssistantErrorElement* error_element) {
  content_view()->AddChildView(
      std::make_unique<AssistantErrorElementView>(error_element));
}

}  //  namespace ash
