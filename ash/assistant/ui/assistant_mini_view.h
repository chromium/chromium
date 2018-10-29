// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_

#include <memory>
#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantController;

// AssistantMiniViewDelegate ---------------------------------------------------

class AssistantMiniViewDelegate {
 public:
  // Invoked when the AssistantMiniView is pressed.
  virtual void OnAssistantMiniViewPressed() {}

 protected:
  virtual ~AssistantMiniViewDelegate() = default;
};

// AssistantMiniView -----------------------------------------------------------

class AssistantMiniView : public views::Button,
                          public views::ButtonListener,
                          public AssistantInteractionModelObserver,
                          public AssistantUiModelObserver {
 public:
  explicit AssistantMiniView(AssistantController* assistant_controller);
  ~AssistantMiniView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnResponseChanged(
      const std::shared_ptr<AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

  void set_delegate(AssistantMiniViewDelegate* delegate) {
    delegate_ = delegate;
  }

 private:
  void InitLayout();
  void UpdatePrompt();

  AssistantController* const assistant_controller_;  // Owned by Shell.
  views::Label* label_;                              // Owned by view hierarchy.

  AssistantMiniViewDelegate* delegate_ = nullptr;

  // The most recent active query for the current Assistant UI session. If there
  // has been no active query for the current UI session, this is empty.
  base::Optional<std::string> last_active_query_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMiniView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
