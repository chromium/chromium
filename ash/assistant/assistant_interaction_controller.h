// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_response_processor.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/highlighter/highlighter_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantController;
class AssistantInteractionModelObserver;
class AssistantResponseProcessor;

class AssistantInteractionController
    : public chromeos::assistant::mojom::AssistantInteractionSubscriber,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantResponseObserver,
      public AssistantUiModelObserver,
      public HighlighterController::Observer,
      public DialogPlateObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;
  using AssistantInteractionResolution =
      chromeos::assistant::mojom::AssistantInteractionResolution;

  explicit AssistantInteractionController(
      AssistantController* assistant_controller);
  ~AssistantInteractionController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns a reference to the underlying model.
  const AssistantInteractionModel* model() const { return &model_; }

  // Adds/removes the specified interaction model |observer|.
  void AddModelObserver(AssistantInteractionModelObserver* observer);
  void RemoveModelObserver(AssistantInteractionModelObserver* observer);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnMicStateChanged(MicState mic_state) override;
  void OnResponseChanged(
      const std::shared_ptr<AssistantResponse>& response) override;

  // AssistantResponseObserver:
  void OnResponseDestroying(AssistantResponse& response) override;

  // AssistantUiModelObserver:
  void OnUiModeChanged(AssistantUiMode ui_mode) override;
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;
  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override;

  // chromeos::assistant::mojom::AssistantInteractionSubscriber:
  void OnInteractionStarted(bool is_voice_interaction) override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;
  void OnSuggestionsResponse(
      std::vector<AssistantSuggestionPtr> response) override;
  void OnTextResponse(const std::string& response) override;
  void OnOpenUrlResponse(const GURL& url) override;
  void OnSpeechRecognitionStarted() override;
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;
  void OnSpeechRecognitionEndOfUtterance() override;
  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;
  void OnSpeechLevelUpdated(float speech_level) override;
  void OnTtsStarted(bool due_to_error) override;

  // DialogPlateObserver:
  void OnDialogPlateButtonPressed(DialogPlateButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;

  // Invoked on suggestion chip pressed event.
  void OnSuggestionChipPressed(const AssistantSuggestion* suggestion);

 private:
  bool HasUnprocessedPendingResponse();

  void OnProcessPendingResponse();
  void OnPendingResponseProcessed(bool success);

  void OnUiVisible(AssistantSource source);

  void StartMetalayerInteraction(const gfx::Rect& region);
  void StartScreenContextInteraction();
  void StartTextInteraction(const std::string text);
  void StartVoiceInteraction();
  void StopActiveInteraction(bool cancel_conversation);

  void OpenUrl(const GURL& url);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  mojo::Binding<chromeos::assistant::mojom::AssistantInteractionSubscriber>
      assistant_interaction_subscriber_binding_;

  AssistantResponseProcessor assistant_response_processor_;

  AssistantInteractionModel model_;

  base::WeakPtrFactory<AssistantInteractionController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_
