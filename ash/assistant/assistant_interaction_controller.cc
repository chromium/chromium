// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns true if device is in tablet mode, false otherwise.
bool IsTabletMode() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

}  // namespace

// AssistantInteractionController ----------------------------------------------

AssistantInteractionController::AssistantInteractionController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_interaction_subscriber_binding_(this),
      assistant_response_processor_(assistant_controller),
      weak_factory_(this) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantInteractionController::~AssistantInteractionController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantInteractionController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant interaction events.
  chromeos::assistant::mojom::AssistantInteractionSubscriberPtr ptr;
  assistant_interaction_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantInteractionSubscriber(std::move(ptr));
}

void AssistantInteractionController::AddModelObserver(
    AssistantInteractionModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantInteractionController::RemoveModelObserver(
    AssistantInteractionModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantInteractionController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantInteractionController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantInteractionController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type == DeepLinkType::kWhatsOnMyScreen) {
    StartScreenContextInteraction();
    return;
  }

  if (type != DeepLinkType::kQuery)
    return;

  const base::Optional<std::string>& query =
      GetDeepLinkParam(params, DeepLinkParam::kQuery);

  if (!query.has_value())
    return;

  // If we receive an empty query, that's a bug that needs to be fixed by the
  // deep link sender. Rather than getting ourselves into a bad state, we'll
  // ignore the deep link.
  if (query.value().empty()) {
    LOG(ERROR) << "Ignoring deep link containing empty query.";
    return;
  }

  StartTextInteraction(query.value());
}

void AssistantInteractionController::OnUiModeChanged(AssistantUiMode ui_mode) {
  if (ui_mode == AssistantUiMode::kMiniUi)
    return;

  switch (model_.input_modality()) {
    case InputModality::kStylus:
      // When the Assistant is not in mini state there should not be an active
      // metalayer session. If we were in mini state when the UI mode was
      // changed, we need to clean up the metalayer session and reset default
      // input modality.
      Shell::Get()->highlighter_controller()->AbortSession();
      model_.SetInputModality(InputModality::kKeyboard);
      break;
    case InputModality::kVoice:
      // When transitioning to web UI we abort any in progress voice query. We
      // do this to prevent Assistant from listening to the user while we
      // navigate away from the main stage.
      if (ui_mode == AssistantUiMode::kWebUi &&
          model_.pending_query().type() == AssistantQueryType::kVoice) {
        StopActiveInteraction(false);
      }
      break;
    case InputModality::kKeyboard:
      // No action necessary.
      break;
  }
}

void AssistantInteractionController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    AssistantSource source) {
  switch (new_visibility) {
    case AssistantVisibility::kClosed:
      // When the UI is closed we need to stop any active interaction. We also
      // reset the interaction state and restore the default input modality.
      StopActiveInteraction(true);
      model_.ClearInteraction();
      model_.SetInputModality(InputModality::kKeyboard);
      break;
    case AssistantVisibility::kHidden:
      // When the UI is hidden we stop any voice query in progress so that we
      // don't listen to the user while not visible. We also restore the default
      // input modality for the next launch.
      if (model_.pending_query().type() == AssistantQueryType::kVoice) {
        StopActiveInteraction(false);
      }
      model_.SetInputModality(InputModality::kKeyboard);
      break;
    case AssistantVisibility::kVisible:
      OnUiVisible(source);
      break;
  }
}

void AssistantInteractionController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      model_.SetInputModality(InputModality::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      FALLTHROUGH;
    case HighlighterEnabledState::kDisabledBySessionComplete:
      model_.SetInputModality(InputModality::kKeyboard);
      break;
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // When metalayer mode has been aborted, no action necessary. Abort occurs
      // as a result of an interaction starting, most likely due to hotword
      // detection. Setting the input modality in these cases would have the
      // unintended consequence of stopping the active interaction.
      break;
  }
}

void AssistantInteractionController::OnHighlighterSelectionRecognized(
    const gfx::Rect& rect) {
  StartMetalayerInteraction(/*region=*/rect);
}

void AssistantInteractionController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // Metalayer mode should not be sticky. Disable it on interaction start.
  Shell::Get()->highlighter_controller()->AbortSession();
}

void AssistantInteractionController::OnInputModalityChanged(
    InputModality input_modality) {
  if (assistant_controller_->ui_controller()->model()->visibility() !=
      AssistantVisibility::kVisible) {
    return;
  }

  if (input_modality == InputModality::kVoice)
    return;

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any pending query. We do not do this when switching to
  // voice input modality because initiation of a voice interaction will
  // automatically interrupt any pre-existing activity. Stopping the active
  // interaction here for voice input modality would actually have the undesired
  // effect of stopping the voice interaction.
  StopActiveInteraction(false);
}

void AssistantInteractionController::OnMicStateChanged(MicState mic_state) {
  // We should stop ChromeVox from speaking when opening the mic.
  if (mic_state == MicState::kOpen)
    Shell::Get()->accessibility_controller()->SilenceSpokenFeedback();
}

void AssistantInteractionController::OnResponseChanged(
    const std::shared_ptr<AssistantResponse>& response) {
  assistant::util::IncrementAssistantQueryCountForEntryPoint(
      assistant_controller_->ui_controller()->model()->entry_point());
}

void AssistantInteractionController::OnResponseDestroying(
    AssistantResponse& response) {
  response.RemoveObserver(this);

  // We need to explicitly clean up resources owned by WebContentsManager for
  // any card elements belonging to the response being destroyed.
  std::vector<base::UnguessableToken> id_tokens;
  for (const auto& ui_element : response.GetUiElements()) {
    if (ui_element->GetType() == AssistantUiElementType::kCard) {
      id_tokens.push_back(
          static_cast<const AssistantCardElement*>(ui_element.get())
              ->id_token());
    }
  }

  if (!id_tokens.empty())
    assistant_controller_->ReleaseWebContents(id_tokens);
}

void AssistantInteractionController::OnInteractionStarted(
    bool is_voice_interaction) {
  if (is_voice_interaction) {
    // If the Assistant UI is not visible yet, and |is_voice_interaction| is
    // true, then it will be sure that Assistant is fired via OKG. ShowUi will
    // not update the Assistant entry point if the UI is already visible.
    assistant_controller_->ui_controller()->ShowUi(AssistantSource::kHotword);
  }

  model_.SetInteractionState(InteractionState::kActive);

  // In the case of a voice interaction, we assume that the mic is open and
  // transition to voice input modality.
  if (is_voice_interaction) {
    model_.SetInputModality(InputModality::kVoice);
    model_.SetMicState(MicState::kOpen);

    // When a voice interaction is initiated by hotword, we haven't yet set a
    // pending query so this is our earliest opportunity.
    if (model_.pending_query().type() == AssistantQueryType::kNull) {
      model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>());
    }
  } else {
    // TODO(b/112000321): It should not be possible to reach this code without
    // having previously pended a query. It does currently happen, however, in
    // the case of notifications and device action queries which bypass the
    // AssistantInteractionController when beginning an interaction. To address
    // this, we temporarily pend an empty text query to commit until we can do
    // development to expose something more meaningful.
    if (model_.pending_query().type() == AssistantQueryType::kNull) {
      model_.SetPendingQuery(std::make_unique<AssistantTextQuery>());
    }

    model_.CommitPendingQuery();
    model_.SetMicState(MicState::kClosed);
  }

  // Start caching a new Assistant response for the interaction. We observe the
  // response so that we can receive notification of lifecycle change events.
  model_.SetPendingResponse(std::make_unique<AssistantResponse>());
  model_.pending_response()->AddObserver(this);
}

void AssistantInteractionController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  model_.SetInteractionState(InteractionState::kInactive);
  model_.SetMicState(MicState::kClosed);

  // If the interaction was finished due to mic timeout, we only want to clear
  // the pending query/response state for that interaction.
  if (resolution == AssistantInteractionResolution::kMicTimeout) {
    model_.ClearPendingQuery();
    model_.ClearPendingResponse();
    return;
  }

  // In normal interaction flows the pending query has already been committed.
  // In some irregular cases, however, it has not. This happens during multi-
  // device hotword loss, for example, but can also occur if the interaction
  // errors out. In these cases we still need to commit the pending query as
  // this is a prerequisite step to being able to finalize the pending response.
  if (model_.pending_query().type() != AssistantQueryType::kNull)
    model_.CommitPendingQuery();

  // It's possible that the pending response has already been finalized. This
  // occurs if the response contained TTS, as we flush the response to the UI
  // when TTS is started to reduce latency.
  if (!model_.pending_response())
    return;

  // Some interaction resolutions require special handling.
  switch (resolution) {
    case AssistantInteractionResolution::kError:
      // In the case of error, we show an appropriate message to the user.
      model_.pending_response()->AddUiElement(
          std::make_unique<AssistantTextElement>(
              l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_ERROR_GENERIC)));
      break;
    case AssistantInteractionResolution::kMultiDeviceHotwordLoss:
      // In the case of hotword loss to another device, we show an appropriate
      // message to the user.
      model_.pending_response()->AddUiElement(
          std::make_unique<AssistantTextElement>(l10n_util::GetStringUTF8(
              IDS_ASH_ASSISTANT_MULTI_DEVICE_HOTWORD_LOSS)));
      break;
    case AssistantInteractionResolution::kMicTimeout:
      // Interactions resolving due to mic timeout are already handled above
      // outside the switch.
      NOTREACHED();
      break;
    case AssistantInteractionResolution::kInterruption:  // fallthrough
    case AssistantInteractionResolution::kNormal:
      // No special handling required.
      break;
  }

  // Perform processing on the pending response before flushing to UI.
  OnProcessPendingResponse();
}

void AssistantInteractionController::OnHtmlResponse(
    const std::string& response,
    const std::string& fallback) {
  if (model_.interaction_state() != InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!HasUnprocessedPendingResponse()) {
    NOTREACHED();
    return;
  }

  model_.pending_response()->AddUiElement(
      std::make_unique<AssistantCardElement>(response, fallback));
}

void AssistantInteractionController::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    assistant_controller_->OpenUrl(suggestion->action_url);
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  StartTextInteraction(suggestion->text);
}

void AssistantInteractionController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  if (model_.interaction_state() != InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!HasUnprocessedPendingResponse()) {
    NOTREACHED();
    return;
  }

  model_.pending_response()->AddSuggestions(std::move(response));
}

void AssistantInteractionController::OnTextResponse(
    const std::string& response) {
  if (model_.interaction_state() != InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!HasUnprocessedPendingResponse()) {
    NOTREACHED();
    return;
  }

  model_.pending_response()->AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantInteractionController::OnSpeechRecognitionStarted() {}

void AssistantInteractionController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>(
      high_confidence_text, low_confidence_text));
}

void AssistantInteractionController::OnSpeechRecognitionEndOfUtterance() {
  model_.SetMicState(MicState::kClosed);
}

void AssistantInteractionController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  // We sometimes receive this event with an empty payload when the interaction
  // is resolving due to mic timeout. In such cases, we should not commit the
  // pending query as the interaction will be discarded.
  if (final_result.empty())
    return;

  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>(final_result));
  model_.CommitPendingQuery();
}

void AssistantInteractionController::OnSpeechLevelUpdated(float speech_level) {
  model_.SetSpeechLevel(speech_level);
}

void AssistantInteractionController::OnTtsStarted(bool due_to_error) {
  if (model_.interaction_state() != InteractionState::kActive) {
    return;
  }

  // Commit the pending query in whatever state it's in. In most cases the
  // pending query is already committed, but we must always commit the pending
  // query before finalizing a pending result.
  if (model_.pending_query().type() != AssistantQueryType::kNull) {
    model_.CommitPendingQuery();
  }

  if (due_to_error) {
    // In the case of an error occurring during a voice interaction, this is our
    // earliest indication that the mic has closed.
    model_.SetMicState(MicState::kClosed);

    // Add an error message to the response.
    model_.pending_response()->AddUiElement(
        std::make_unique<AssistantTextElement>(
            l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_ERROR_GENERIC)));
  }

  model_.pending_response()->set_has_tts(true);
  // We have an agreement with the server that TTS will always be the last part
  // of an interaction to be processed. To be timely in updating UI, we use
  // this as an opportunity to begin processing the Assistant response.
  OnProcessPendingResponse();
}

void AssistantInteractionController::OnOpenUrlResponse(const GURL& url) {
  if (model_.interaction_state() != InteractionState::kActive) {
    return;
  }
  // We need to indicate that the navigation attempt is occurring as a result of
  // a server response so that we can differentiate from navigation attempts
  // initiated by direct user interaction.
  assistant_controller_->OpenUrl(url, /*from_server=*/true);
}

void AssistantInteractionController::OnDialogPlateButtonPressed(
    DialogPlateButtonId id) {
  if (id == DialogPlateButtonId::kKeyboardInputToggle) {
    model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  if (id != DialogPlateButtonId::kVoiceInputToggle)
    return;

  switch (model_.mic_state()) {
    case MicState::kClosed:
      StartVoiceInteraction();
      break;
    case MicState::kOpen:
      StopActiveInteraction(false);
      break;
  }
}

void AssistantInteractionController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  DCHECK(!text.empty());
  StartTextInteraction(text);
}

bool AssistantInteractionController::HasUnprocessedPendingResponse() {
  return model_.pending_response() &&
         model_.pending_response()->processing_state() ==
             AssistantResponse::ProcessingState::kUnprocessed;
}

void AssistantInteractionController::OnProcessPendingResponse() {
  // It's possible that the pending response is already being processed. This
  // can occur if the response contains TTS, as we begin processing before the
  // interaction is finished in such cases to reduce UI latency.
  if (model_.pending_response()->processing_state() !=
      AssistantResponse::ProcessingState::kUnprocessed) {
    return;
  }

  // Start processing.
  assistant_response_processor_.Process(
      *model_.pending_response(),
      base::BindOnce(
          &AssistantInteractionController::OnPendingResponseProcessed,
          weak_factory_.GetWeakPtr()));
}

void AssistantInteractionController::OnPendingResponseProcessed(bool success) {
  if (!success)
    return;

  // Once the pending response has been processed it is safe to flush to the UI.
  // We accomplish this by finalizing the pending response.
  model_.FinalizePendingResponse();
}

void AssistantInteractionController::OnUiVisible(AssistantSource source) {
  DCHECK_EQ(AssistantVisibility::kVisible,
            assistant_controller_->ui_controller()->model()->visibility());

  switch (source) {
    case AssistantSource::kHotkey:
    case AssistantSource::kLauncherSearchBox:
    case AssistantSource::kLongPressLauncher: {
      // When the user prefers it or when we are in tablet mode, launching
      // Assistant UI will immediately start a voice interaction.
      const bool launch_with_mic_open =
          Shell::Get()->voice_interaction_controller()->launch_with_mic_open();
      if (launch_with_mic_open || IsTabletMode())
        StartVoiceInteraction();
      break;
    }
    case AssistantSource::kStylus:
      model_.SetInputModality(InputModality::kStylus);
      break;
    case AssistantSource::kUnspecified:
    case AssistantSource::kDeepLink:
    case AssistantSource::kHotword:
    case AssistantSource::kSetup:
      // No action necessary.
      break;
  }
}

void AssistantInteractionController::StartMetalayerInteraction(
    const gfx::Rect& region) {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
      l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN)));

  assistant_->StartMetalayerInteraction(region);
}

void AssistantInteractionController::StartScreenContextInteraction() {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
      l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN)));

  // Note that screen context was cached when the UI was launched.
  assistant_->StartCachedScreenContextInteraction();
}

void AssistantInteractionController::StartTextInteraction(
    const std::string text) {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(text));

  assistant_->SendTextQuery(text);
}

void AssistantInteractionController::StartVoiceInteraction() {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>());

  assistant_->StartVoiceInteraction();
}

void AssistantInteractionController::StopActiveInteraction(
    bool cancel_conversation) {
  // Even though the interaction state will be asynchronously set to inactive
  // via a call to OnInteractionFinished(Resolution), we explicitly set it to
  // inactive here to prevent processing any additional UI related service
  // events belonging to the interaction being stopped.
  model_.SetInteractionState(InteractionState::kInactive);
  model_.ClearPendingQuery();

  assistant_->StopActiveInteraction(cancel_conversation);

  // Because we are stopping an interaction in progress, we discard any pending
  // response for it that is cached to prevent it from being finalized when the
  // interaction is finished.
  model_.ClearPendingResponse();
}

}  // namespace ash
