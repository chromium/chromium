// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ERROR_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ERROR_ELEMENT_VIEW_H_

#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "base/component_export.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// AssistantErrorElementView is the visual representation of an
// AssistantErrorElement. AssistantErrorElementView uses the same rendering
// logic as AssistantTextElementView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantErrorElementView
    : public AssistantTextElementView {
  METADATA_HEADER(AssistantErrorElementView, AssistantTextElementView)
 public:
  explicit AssistantErrorElementView(
      const AssistantErrorElement* error_element);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ERROR_ELEMENT_VIEW_H_
