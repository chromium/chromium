// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_

#include <memory>
#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// AssistantMiniView -----------------------------------------------------------

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantMiniView
    : public views::Button,
      public views::ButtonListener,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantMiniView(AssistantViewDelegate* delegate);
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
      const scoped_refptr<AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void UpdatePrompt();

  AssistantViewDelegate* const delegate_;
  views::Label* label_;                              // Owned by view hierarchy.

  // The most recent active query for the current Assistant UI session. If there
  // has been no active query for the current UI session, this is empty.
  base::Optional<std::string> last_active_query_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMiniView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
