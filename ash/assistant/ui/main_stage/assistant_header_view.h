// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_

#include <memory>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class AssistantController;
class BaseLogoView;

// AssistantHeaderView is the child of UiElementContainerView which provides
// the Assistant icon.
class AssistantHeaderView : public views::View,
                            public AssistantInteractionModelObserver,
                            public AssistantUiModelObserver {
 public:
  explicit AssistantHeaderView(AssistantController* assistant_controller);
  ~AssistantHeaderView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  // AssistantInteractionModelObserver:
  void OnResponseChanged(
      const std::shared_ptr<AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.
  BaseLogoView* molecule_icon_;       // Owned by view hierarchy.

  // True if this is the first query response received for the current Assistant
  // UI session, false otherwise.
  bool is_first_response_ = true;

  DISALLOW_COPY_AND_ASSIGN(AssistantHeaderView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
