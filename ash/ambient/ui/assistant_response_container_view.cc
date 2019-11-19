// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/assistant_response_container_view.h"

#include <memory>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredWidthDip = 600;

}  // namespace

AssistantResponseContainerView::AssistantResponseContainerView(
    AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
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

void AssistantResponseContainerView::HandleResponse(
    const AssistantResponse& response) {
  for (const auto& ui_element : response.GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        // For card elements, we instead use the "fallback" message for HTML
        // card rendering as the text response.
        AddTextElementView(new AssistantTextElement(
            static_cast<const AssistantCardElement*>(ui_element.get())
                ->fallback()));
        break;
      case AssistantUiElementType::kText:
        AddTextElementView(
            static_cast<const AssistantTextElement*>(ui_element.get()));
        break;
    }
  }
}

void AssistantResponseContainerView::AddTextElementView(
    const AssistantTextElement* text_element) {
  content_view()->AddChildView(
      std::make_unique<AssistantTextElementView>(text_element));
}

}  //  namespace ash
