// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_

#include <memory>

#include "base/component_export.h"

namespace ash {

class AssistantUiElement;
class AssistantUiElementView;
class AssistantViewDelegate;

// Factory class which creates Assistant views for modeled UI elements.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantUiElementViewFactory {
 public:
  explicit AssistantUiElementViewFactory(AssistantViewDelegate* delegate);
  AssistantUiElementViewFactory(AssistantViewDelegate& copy) = delete;
  AssistantUiElementViewFactory operator=(
      AssistantUiElementViewFactory& assign) = delete;
  ~AssistantUiElementViewFactory();

  // Creates a view for the specified |ui_element|.
  std::unique_ptr<AssistantUiElementView> Create(
      const AssistantUiElement* ui_element) const;

 private:
  // Owned by AssistantController.
  AssistantViewDelegate* const delegate_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_
