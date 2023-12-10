// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/scoped_observation.h"

namespace ash {

// The implementation of the Assistant controller in charge of suggestions.
class AssistantSuggestionsControllerImpl
    : public AssistantSuggestionsController,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public AssistantStateObserver {
 public:
  AssistantSuggestionsControllerImpl();

  AssistantSuggestionsControllerImpl(
      const AssistantSuggestionsControllerImpl&) = delete;
  AssistantSuggestionsControllerImpl& operator=(
      const AssistantSuggestionsControllerImpl&) = delete;

  ~AssistantSuggestionsControllerImpl() override;

  // AssistantSuggestionsController:
  const AssistantSuggestionsModel* GetModel() const override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

 private:
  // AssistantStateObserver:
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantOnboardingModeChanged(
      assistant::prefs::AssistantOnboardingMode onboarding_mode) override;

  void UpdateConversationStarters();
  void UpdateOnboardingSuggestions();

  AssistantSuggestionsModel model_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  // A WeakPtrFactory used to manage lifecycle of conversation starter requests
  // to the server (via the dedicated ConversationStartersClient).
  base::WeakPtrFactory<AssistantSuggestionsControllerImpl>
      conversation_starters_weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_
