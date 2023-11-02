// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_ui_element.h"

#include <utility>

namespace ash {

AssistantUiElement::AssistantUiElement(AssistantUiElementType type)
    : type_(type) {}

AssistantUiElement::~AssistantUiElement() = default;

void AssistantUiElement::Process(ProcessingCallback callback) {
  // By default, Assistant UI elements do not require pre-rendering processing.
  std::move(callback).Run();
}

}  // namespace ash
