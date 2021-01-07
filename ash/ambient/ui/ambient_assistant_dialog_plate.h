// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_
#define ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/base/assistant_button_listener.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class AssistantQueryView;
class AssistantViewDelegate;
class MicView;

class AmbientAssistantDialogPlate : public views::View,
                                    public AssistantButtonListener,
                                    public AssistantControllerObserver,
                                    public AssistantInteractionModelObserver {
 public:
  METADATA_HEADER(AmbientAssistantDialogPlate);

  explicit AmbientAssistantDialogPlate(AssistantViewDelegate* delegate);
  ~AmbientAssistantDialogPlate() override;

  // AssistantButtonListener:
  void OnButtonPressed(AssistantButtonId button_id) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryChanged(const AssistantQuery& query) override;

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;

  // Owned by view hierarchy.
  MicView* animated_voice_input_toggle_ = nullptr;
  AssistantQueryView* voice_query_view_ = nullptr;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ASSISTANT_DIALOG_PLATE_H_
