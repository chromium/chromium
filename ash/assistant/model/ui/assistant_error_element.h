// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_ERROR_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_ERROR_ELEMENT_H_

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/component_export.h"

namespace ash {

// An Assistant UI error element that will be rendered as text.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantErrorElement
    : public AssistantUiElement {
 public:
  explicit AssistantErrorElement(int message_id);

  AssistantErrorElement(const AssistantErrorElement&) = delete;
  AssistantErrorElement& operator=(const AssistantErrorElement&) = delete;

  ~AssistantErrorElement() override;

  int message_id() const { return message_id_; }

 private:
  const int message_id_;

  // AssistantUiElement:
  bool Compare(const AssistantUiElement& other) const override;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_ERROR_ELEMENT_H_
