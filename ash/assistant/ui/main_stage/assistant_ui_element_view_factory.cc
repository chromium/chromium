// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_ui_element_view_factory.h"

#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_text_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_error_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "base/logging.h"

namespace ash {

AssistantUiElementViewFactory::AssistantUiElementViewFactory(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {}

AssistantUiElementViewFactory::~AssistantUiElementViewFactory() = default;

std::unique_ptr<AssistantUiElementView> AssistantUiElementViewFactory::Create(
    const AssistantUiElement* ui_element) const {
  switch (ui_element->type()) {
    case AssistantUiElementType::kCard: {
      auto* card_element = static_cast<const AssistantCardElement*>(ui_element);
      if (!card_element->has_contents_view()) {
        // TODO(b/228109139): Find the root cause why reaches here.
        LOG(DFATAL) << "AssistantCardElement has null contents_view. Not "
                       "create the view.";
        return nullptr;
      }
      return std::make_unique<AssistantCardElementView>(delegate_,
                                                        card_element);
    }
    case AssistantUiElementType::kError:
      return std::make_unique<AssistantErrorElementView>(
          static_cast<const AssistantErrorElement*>(ui_element));
    case AssistantUiElementType::kText:
      return std::make_unique<AssistantTextElementView>(
          static_cast<const AssistantTextElement*>(ui_element));
  }
}

}  // namespace ash
