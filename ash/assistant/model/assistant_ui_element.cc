// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_element.h"

namespace ash {

// AssistantCardElement --------------------------------------------------------

AssistantCardElement::AssistantCardElement(const std::string& html,
                                           const std::string& fallback)
    : AssistantUiElement(AssistantUiElementType::kCard),
      html_(html),
      fallback_(fallback),
      id_token_(base::UnguessableToken::Create()) {}

AssistantCardElement::~AssistantCardElement() = default;

}  // namespace ash
