// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_error_element.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AssistantErrorElement::AssistantErrorElement(int message_id)
    : AssistantUiElement(AssistantUiElementType::kError),
      message_id_(message_id) {}

AssistantErrorElement::~AssistantErrorElement() = default;

bool AssistantErrorElement::Compare(const AssistantUiElement& other) const {
  return other.type() == AssistantUiElementType::kError &&
         static_cast<const AssistantErrorElement&>(other).message_id() ==
             message_id_;
}
}  // namespace ash
