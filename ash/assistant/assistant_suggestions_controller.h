// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_

#include <memory>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_proactive_suggestions_controller.h"
#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/macros.h"

namespace ash {

class AssistantController;
class AssistantSuggestionsModelObserver;
class ProactiveSuggestions;

class AssistantSuggestionsController : public AssistantControllerObserver,
                                       public AssistantUiModelObserver,
                                       public AssistantStateObserver {
 public:
  explicit AssistantSuggestionsController(
      AssistantController* assistant_controller);
  ~AssistantSuggestionsController() override;

  // Returns a reference to the underlying model.
  const AssistantSuggestionsModel* model() const { return &model_; }

  // Adds/removes the specified suggestions model |observer|.
  void AddModelObserver(AssistantSuggestionsModelObserver* observer);
  void RemoveModelObserver(AssistantSuggestionsModelObserver* observer);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Invoked when the active set of |proactive_suggestions| has changed. Note
  // that this method should only be called by the sub-controller for the
  // proactive suggestions feature to update model state.
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions);

 private:
  // AssistantStateObserver:
  void OnAssistantContextEnabled(bool enabled) override;

  void UpdateConversationStarters();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // A sub-controller for the proactive suggestions feature. Note that this will
  // only exist if the proactive suggestions feature is enabled.
  std::unique_ptr<AssistantProactiveSuggestionsController>
      proactive_suggestions_controller_;

  AssistantSuggestionsModel model_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSuggestionsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
