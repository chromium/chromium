// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_text_element.h"

#include "ash/assistant/ui/assistant_ui_constants.h"

namespace ash {

AssistantTextElement::AssistantTextElement(const std::string& text)
    : AssistantUiElement(AssistantUiElementType::kText), text_(text) {}

AssistantTextElement::~AssistantTextElement() = default;

bool AssistantTextElement::Compare(const AssistantUiElement& other) const {
  return other.type() == AssistantUiElementType::kText &&
         static_cast<const AssistantTextElement&>(other).text() == text_;
}

}  // namespace ash
