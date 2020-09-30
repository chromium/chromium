// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefRegistrySimple;

namespace ash {

class AssistantControllerImpl;
enum class AssistantButtonId;

class AssistantInteractionControllerImpl
    : public AssistantInteractionController,
      public chromeos::assistant::AssistantInteractionSubscriber,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver,
      public TabletModeObserver,
      public HighlighterController::Observer {
 public:
  using AssistantInteractionMetadata =
      chromeos::assistant::AssistantInteractionMetadata;
  using AssistantInteractionResolution =
      chromeos::assistant::AssistantInteractionResolution;
  using AssistantInteractionType =
      chromeos::assistant::AssistantInteractionType;
  using AssistantQuerySource = chromeos::assistant::AssistantQuerySource;
  using AssistantSuggestion = chromeos::assistant::AssistantSuggestion;
  using AssistantSuggestionType = chromeos::assistant::AssistantSuggestionType;

  explicit AssistantInteractionControllerImpl(
      AssistantControllerImpl* assistant_controller);
  ~AssistantInteractionControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(chromeos::assistant::Assistant* assistant);

  // AssistantInteractionController:
  const AssistantInteractionModel* GetModel() const override;
  base::TimeDelta GetTimeDeltaSinceLastInteraction() const override;
  bool HasHadInteraction() const override;
  void StartTextInteraction(const std::string& text,
                            bool allow_tts,
                            AssistantQuerySource query_source) override;
  void StartBloomInteraction() override;
  void ShowBloomResult(const std::string& html) override;

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
  void OnCommittedQueryChanged(const AssistantQuery& assistant_query) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // HighlighterController::Observer:
  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override;

  // chromeos::assistant::AssistantInteractionSubscriber:
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
  bool OnOpenAppResponse(
      const chromeos::assistant::AndroidAppInfo& app_info) override;
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

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  void OnTabletModeChanged();

  bool HasUnprocessedPendingResponse();
  bool HasActiveInteraction() const;

  void OnProcessPendingResponse();
  void OnPendingResponseProcessed(bool is_completed);

  void OnUiVisible(AssistantEntryPoint entry_point);

  void StartScreenContextInteraction(bool include_assistant_structure,
                                     const gfx::Rect& region,
                                     AssistantQuerySource query_source);
  void StartVoiceInteraction();
  void StopActiveInteraction(bool cancel_conversation);

  InputModality GetDefaultInputModality() const;
  AssistantResponse* GetResponseForActiveInteraction();
  AssistantVisibility GetVisibility() const;
  bool IsVisible() const;

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.
  AssistantInteractionModel model_;
  bool has_had_interaction_ = false;

  // Owned by AssistantService.
  chromeos::assistant::Assistant* assistant_ = nullptr;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<HighlighterController, HighlighterController::Observer>
      highlighter_controller_observer_{this};

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_controller_observer_{this};

  base::WeakPtrFactory<AssistantInteractionControllerImpl>
      screen_context_request_factory_{this};
  base::WeakPtrFactory<AssistantInteractionControllerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionControllerImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_
