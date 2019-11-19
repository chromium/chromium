// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_
#define ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class AssistantQueryView;
class AssistantViewDelegate;
class MicView;
enum class AssistantButtonId;

class AmbientAssistantDialogPlate : public views::View,
                                    public views::ButtonListener,
                                    public AssistantInteractionModelObserver {
 public:
  explicit AmbientAssistantDialogPlate(AssistantViewDelegate* delegate);
  ~AmbientAssistantDialogPlate() override;

  // views::View:
  const char* GetClassName() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryChanged(const AssistantQuery& query) override;

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;

  // Owned by view hierarchy.
  MicView* animated_voice_input_toggle_ = nullptr;
  AssistantQueryView* voice_query_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AmbientAssistantDialogPlate);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_
