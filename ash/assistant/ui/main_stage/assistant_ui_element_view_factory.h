// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

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
  const raw_ptr<AssistantViewDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_FACTORY_H_
