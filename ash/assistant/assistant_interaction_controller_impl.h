// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

class PrefRegistrySimple;

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class AssistantControllerImpl;
enum class AssistantButtonId;

class AssistantInteractionControllerImpl
    : public AssistantInteractionController,
      public assistant::AssistantInteractionSubscriber,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver,
      public display::DisplayObserver {
 public:
  using AssistantInteractionMetadata = assistant::AssistantInteractionMetadata;
  using AssistantInteractionResolution =
      assistant::AssistantInteractionResolution;
  using AssistantInteractionType = assistant::AssistantInteractionType;
  using AssistantQuerySource = assistant::AssistantQuerySource;
  using AssistantSuggestion = assistant::AssistantSuggestion;
  using AssistantSuggestionType = assistant::AssistantSuggestionType;

  explicit AssistantInteractionControllerImpl(
      AssistantControllerImpl* assistant_controller);

  AssistantInteractionControllerImpl(
      const AssistantInteractionControllerImpl&) = delete;
  AssistantInteractionControllerImpl& operator=(
      const AssistantInteractionControllerImpl&) = delete;

  ~AssistantInteractionControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(assistant::Assistant* assistant);

  // AssistantInteractionController:
  const AssistantInteractionModel* GetModel() const override;
  base::TimeDelta GetTimeDeltaSinceLastInteraction() const override;
  bool HasHadInteraction() const override;
  void StartTextInteraction(const std::string& text,
                            bool allow_tts,
                            AssistantQuerySource query_source) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnMicStateChanged(MicState mic_state) override;
  void OnCommittedQueryChanged(const AssistantQuery& assistant_query) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // assistant::AssistantInteractionSubscriber:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;
  void OnSuggestionsResponse(
      const std::vector<AssistantSuggestion>& response) override;
  void OnTextResponse(const std::string& response) override;
  void OnOpenUrlResponse(const GURL& url, bool in_background) override;
  void OnOpenAppResponse(const assistant::AndroidAppInfo& app_info) override;
  void OnSpeechRecognitionStarted() override;
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;
  void OnSpeechRecognitionEndOfUtterance() override;
  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;
  void OnSpeechLevelUpdated(float speech_level) override;
  void OnTtsStarted(bool due_to_error) override;
  void OnWaitStarted() override;

  // AssistantViewDelegateObserver:
  void OnDialogPlateButtonPressed(AssistantButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;
  void OnSuggestionPressed(
      const base::UnguessableToken& suggestion_id) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  bool HasActiveInteraction() const;
  void OnUiVisible(AssistantEntryPoint entry_point);
  void StartVoiceInteraction();
  void StopActiveInteraction(bool cancel_conversation);

  InputModality GetDefaultInputModality() const;
  AssistantResponse* GetResponseForActiveInteraction();
  AssistantVisibility GetVisibility() const;
  bool IsVisible() const;

  const raw_ptr<AssistantControllerImpl>
      assistant_controller_;  // Owned by Shell.
  AssistantInteractionModel model_;
  bool has_had_interaction_ = false;

  // Owned by AssistantService.
  raw_ptr<assistant::Assistant> assistant_ = nullptr;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  base::ScopedObservation<display::Screen, display::DisplayObserver>
      display_observation_{this};

  base::WeakPtrFactory<AssistantInteractionControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_
