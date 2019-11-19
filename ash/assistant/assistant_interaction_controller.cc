// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/public/cpp/android_intent_helper.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/features.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr int kWarmerWelcomesMaxTimesTriggered = 3;
constexpr char kAndroidIntentScheme[] = "intent://";
constexpr char kAndroidIntentPrefix[] = "#Intent";

// Helpers ---------------------------------------------------------------------

// Creates a suggestion to initiate a Google search for the specified |query|.
chromeos::assistant::mojom::AssistantSuggestionPtr CreateSearchSuggestion(
    const std::string& query) {
  constexpr char kIconUrl[] =
      "https://www.gstatic.com/images/branding/product/2x/googleg_48dp.png";
  constexpr char kSearchUrl[] = "https://www.google.com/search";
  constexpr char kQueryParamKey[] = "q";

  chromeos::assistant::mojom::AssistantSuggestionPtr suggestion =
      chromeos::assistant::mojom::AssistantSuggestion::New();
  suggestion->text = l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_SEARCH);
  suggestion->icon_url = GURL(kIconUrl),
  suggestion->action_url = net::AppendOrReplaceQueryParameter(
      GURL(kSearchUrl), kQueryParamKey, query);

  return suggestion;
}

// Returns true if device is in tablet mode, false otherwise.
bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

// AssistantInteractionController ----------------------------------------------

AssistantInteractionController::AssistantInteractionController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
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
  assistant_->AddAssistantInteractionSubscriber(
      assistant_interaction_subscriber_receiver_.BindNewPipeAndPassRemote());
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
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantInteractionController::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantInteractionController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type == DeepLinkType::kWhatsOnMyScreen) {
    // Explicitly call ShowUi() to set the correct Assistant entry point.
    // ShowUi() will no-op if UI is already shown.
    assistant_controller_->ui_controller()->ShowUi(
        AssistantEntryPoint::kDeepLink);

    // Currently the only way to trigger this deeplink is via suggestion chip.
    // TODO(b/119841827): Use source specified from deep link.
    StartScreenContextInteraction(AssistantQuerySource::kSuggestionChip);
    return;
  }

  if (type == DeepLinkType::kReminders) {
    using ReminderAction = assistant::util::ReminderAction;
    const base::Optional<ReminderAction>& action =
        GetDeepLinkParamAsRemindersAction(params, DeepLinkParam::kAction);

    // We treat reminders deeplinks without an action as web deep links.
    if (!action)
      return;

    switch (action.value()) {
      case ReminderAction::kCreate:
        StartTextInteraction(
            l10n_util::GetStringUTF8(IDS_ASSISTANT_CREATE_REMINDER_QUERY),
            /*allow_tts=*/model_.response() && model_.response()->has_tts(),
            /*query_source=*/AssistantQuerySource::kDeepLink);
        break;

      case ReminderAction::kEdit:
        const base::Optional<std::string>& client_id =
            GetDeepLinkParam(params, DeepLinkParam::kClientId);
        if (client_id && !client_id.value().empty()) {
          StopActiveInteraction(false);
          model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
              l10n_util::GetStringUTF8(IDS_ASSISTANT_EDIT_REMINDER_QUERY),
              /*query_source=*/AssistantQuerySource::kDeepLink));
          assistant_->StartEditReminderInteraction(client_id.value());
        }
        break;
    }

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

  // Explicitly call ShowUi() to set the correct Assistant entry point.
  // ShowUi() will no-op if UI is already shown.
  assistant_controller_->ui_controller()->ShowUi(
      AssistantEntryPoint::kDeepLink);

  // A text query originating from a deep link will carry forward the allowance/
  // forbiddance of TTS from the previous response. This is predominately aimed
  // at addressing the use case of tapping a card from a previous query response
  // in which case we are essentially continuing the preceding interaction. Deep
  // links are also potentially fired from notifications or other sources. If we
  // need to allow deep link creators the ability to set |allow_tts| explicitly,
  // we can expose a deep link parameter when the need arises.
  StartTextInteraction(query.value(), /*allow_tts=*/model_.response() &&
                                          model_.response()->has_tts(),
                       /*query_source=*/AssistantQuerySource::kDeepLink);
}

void AssistantInteractionController::OnUiModeChanged(AssistantUiMode ui_mode,
                                                     bool due_to_interaction) {
  if (ui_mode == AssistantUiMode::kMiniUi)
    return;

  switch (model_.input_modality()) {
    case InputModality::kStylus:
      // When the Assistant is not in mini state there should not be an active
      // metalayer session. If we were in mini state when the UI mode was
      // changed, we need to clean up the metalayer session.
      Shell::Get()->highlighter_controller()->AbortSession();

      // If the UI mode change was not due to interaction, we should reset the
      // default input modality. We don't do this if the change occurred due to
      // an Assistant interaction because we might inadvertently stop the active
      // interaction by changing input modality. When this is the case, the
      // modality will be correctly set in |OnInteractionStarted| if needed.
      if (!due_to_interaction)
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
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
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
      OnUiVisible(entry_point.value());
      break;
  }
}

void AssistantInteractionController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      // Skip setting input modality to stylus when the embedded Assistant
      // feature is enabled to prevent highlighter aborting sessions in
      // OnUiModeChanged.
      if (!app_list_features::IsAssistantLauncherUIEnabled())
        model_.SetInputModality(InputModality::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
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
  assistant_controller_->ui_controller()->ShowUi(AssistantEntryPoint::kStylus);
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

  // Metalayer interactions cause an input modality change that causes us to
  // lose the pending query. We cache the source before stopping the active
  // interaction so we can restore the pending query when using the stylus.
  const auto source = model_.pending_query().source();

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any pending query. We do not do this when switching to
  // voice input modality because initiation of a voice interaction will
  // automatically interrupt any pre-existing activity. Stopping the active
  // interaction here for voice input modality would actually have the undesired
  // effect of stopping the voice interaction.
  StopActiveInteraction(false);

  if (source == AssistantQuerySource::kStylus) {
    model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
        l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN),
        AssistantQuerySource::kStylus));
  }
}

void AssistantInteractionController::OnMicStateChanged(MicState mic_state) {
  // We should stop ChromeVox from speaking when opening the mic.
  if (mic_state == MicState::kOpen)
    Shell::Get()->accessibility_controller()->SilenceSpokenFeedback();
}

void AssistantInteractionController::OnCommittedQueryChanged(
    const AssistantQuery& assistant_query) {
  std::string query;
  switch (assistant_query.type()) {
    case AssistantQueryType::kText: {
      const auto* assistant_text_query =
          static_cast<const AssistantTextQuery*>(&assistant_query);
      query = assistant_text_query->text();
      break;
    }
    case AssistantQueryType::kVoice: {
      const auto* assistant_voice_query =
          static_cast<const AssistantVoiceQuery*>(&assistant_query);
      query = assistant_voice_query->high_confidence_speech();
      break;
    }
    case AssistantQueryType::kNull:
      break;
  }
  model_.query_history().Add(query);

  assistant::util::IncrementAssistantQueryCountForEntryPoint(
      assistant_controller_->ui_controller()->model()->entry_point());
  assistant::util::RecordAssistantQuerySource(assistant_query.source());
}

// TODO(b/140565663): Set pending query from |metadata| and remove calls to set
// pending query that occur outside of this method.
void AssistantInteractionController::OnInteractionStarted(
    AssistantInteractionMetadataPtr metadata) {
  // Stop the interaction if the opt-in window is active.
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (assistant_setup && assistant_setup->BounceOptInWindowIfActive()) {
    StopActiveInteraction(true);
    return;
  }

  const bool is_voice_interaction =
      chromeos::assistant::mojom::AssistantInteractionType::kVoice ==
      metadata->type;

  if (is_voice_interaction) {
    // If the Assistant UI is not visible yet, and |is_voice_interaction| is
    // true, then it will be sure that Assistant is fired via OKG. ShowUi will
    // not update the Assistant entry point if the UI is already visible.
    assistant_controller_->ui_controller()->ShowUi(
        AssistantEntryPoint::kHotword);
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
    // Once b/140565663 has been addressed to remove all calls which currently
    // set the pending query from outside of the interaction lifecycle, the
    // pending query type will always be |kNull| here.
    if (model_.pending_query().type() == AssistantQueryType::kNull) {
      model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
          metadata->query, metadata->source));
    }
    model_.CommitPendingQuery();
    model_.SetMicState(MicState::kClosed);
  }

  // Start caching a new Assistant response for the interaction.
  model_.SetPendingResponse(base::MakeRefCounted<AssistantResponse>());
}

void AssistantInteractionController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  model_.SetInteractionState(InteractionState::kInactive);
  model_.SetMicState(MicState::kClosed);

  // The mic timeout resolution is delivered inconsistently by LibAssistant. To
  // account for this, we need to check if the interaction resolved normally
  // with an empty voice query and, if so, also treat this as a mic timeout.
  const bool is_mic_timeout =
      resolution == AssistantInteractionResolution::kMicTimeout ||
      (resolution == AssistantInteractionResolution::kNormal &&
       model_.pending_query().type() == AssistantQueryType::kVoice &&
       model_.pending_query().Empty());

  // If the interaction was finished due to mic timeout, we only want to clear
  // the pending query/response state for that interaction.
  if (is_mic_timeout) {
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
    // Note that we post a new task when opening the |action_url| associated
    // with |sugggestion| as this will potentially cause Assistant UI to close
    // and destroy |suggestion| in the process. Failure to post in this case
    // would cause any subsequent observers of this suggestion chip event to
    // receive a deleted pointer.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantController::OpenUrl,
                       assistant_controller_->GetWeakPtr(),
                       suggestion->action_url, /*in_background=*/false,
                       /*from_server=*/false));
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  // Note that a text query originating from a suggestion chip will carry
  // forward the allowance/forbiddance of TTS from the previous response. This
  // is because suggestion chips pressed after a voice query should continue to
  // return TTS, as really the text interaction is just a continuation of the
  // user's preceding voice interaction.
  StartTextInteraction(suggestion->text, /*allow_tts=*/model_.response() &&
                                             model_.response()->has_tts(),
                       /*query_source=*/AssistantQuerySource::kSuggestionChip);
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

    // It is possible that an error Tts could be sent in addition to server Tts.
    // In that case, the pending_response may have already been finalized.
    if (!model_.pending_response())
      model_.SetPendingResponse(base::MakeRefCounted<AssistantResponse>());

    // Add an error message to the response.
    model_.pending_response()->AddUiElement(
        std::make_unique<AssistantTextElement>(
            l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_ERROR_GENERIC)));
  }

  model_.pending_response()->set_has_tts(true);

  // We have an agreement with the server that TTS will always be the last part
  // of an interaction to be processed. To be timely in updating UI, we use
  // this as an opportunity to begin processing the Assistant response.
  // TODO(xiaohuic): sometimes we actually do receive additional TTS responses,
  // need to properly handle those cases.
  OnProcessPendingResponse();
}

void AssistantInteractionController::OnWaitStarted() {
  if (model_.interaction_state() != InteractionState::kActive)
    return;

  // Commit the pending query in whatever state it's in. Note that the server
  // guarantees that if a wait occurs, it is as part of a routine's execution
  // and it will be the last event prior to the current interaction being
  // finished and the response will not contain any TTS. Upon finishing
  // execution of the current interaction, a new interaction will automatically
  // be started for the next leg of the routine's execution.
  if (model_.pending_query().type() != AssistantQueryType::kNull)
    model_.CommitPendingQuery();

  // Finalize the pending response so that the UI is flushed to the screen while
  // the wait occurs, giving the user time to digest the current response before
  // the routine begins its next leg in the next interaction.
  OnProcessPendingResponse();
}

void AssistantInteractionController::OnOpenUrlResponse(const GURL& url,
                                                       bool in_background) {
  if (model_.interaction_state() != InteractionState::kActive)
    return;
  // We need to indicate that the navigation attempt is occurring as a result of
  // a server response so that we can differentiate from navigation attempts
  // initiated by direct user interaction.
  assistant_controller_->OpenUrl(url, in_background, /*from_server=*/true);
}

void AssistantInteractionController::OnOpenAppResponse(
    chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
    OnOpenAppResponseCallback callback) {
  if (model_.interaction_state() != InteractionState::kActive)
    return;

  auto* android_helper = AndroidIntentHelper::GetInstance();
  if (!android_helper) {
    std::move(callback).Run(false);
    return;
  }

  auto intent = android_helper->GetAndroidAppLaunchIntent(std::move(app_info));
  if (!intent.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Common Android intent might starts with intent scheme "intent://" or
  // Android app scheme "android-app://". But it might also only contains
  // reference starts with "#Intent".
  // However, GURL requires the URL spec to be non-empty, which invalidate the
  // intent starts with "#Intent". For this case, we adding the Android intent
  // scheme to the intent to validate it for GURL constructor.
  auto intent_str = intent.value();
  if (base::StartsWith(intent_str, kAndroidIntentPrefix,
                       base::CompareCase::SENSITIVE)) {
    intent_str = kAndroidIntentScheme + intent_str;
  }
  assistant_controller_->OpenUrl(GURL(intent_str), /*in_background=*/false,
                                 /*from_server=*/true);
  std::move(callback).Run(true);
}

void AssistantInteractionController::OnDialogPlateButtonPressed(
    AssistantButtonId id) {
  if (id == AssistantButtonId::kKeyboardInputToggle) {
    model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  if (id != AssistantButtonId::kVoiceInputToggle)
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
  StartTextInteraction(
      text, /*allow_tts=*/false,
      /*query_source=*/AssistantQuerySource::kDialogPlateTextField);
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

  // Bind an interface to a navigable contents factory that is needed for
  // processing card elements.
  mojo::Remote<content::mojom::NavigableContentsFactory> factory;
  assistant_controller_->GetNavigableContentsFactory(
      factory.BindNewPipeAndPassReceiver());

  // Start processing.
  model_.pending_response()->Process(
      std::move(factory),
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

void AssistantInteractionController::OnUiVisible(
    AssistantEntryPoint entry_point) {
  DCHECK_EQ(AssistantVisibility::kVisible,
            assistant_controller_->ui_controller()->model()->visibility());

  const bool launch_with_mic_open =
      AssistantState::Get()->launch_with_mic_open().value_or(false);
  const bool prefer_voice = launch_with_mic_open || IsTabletMode();

  // We don't explicitly start a new voice interaction if the entry point
  // is hotword since in such cases a voice interaction will already be in
  // progress.
  if (assistant::util::IsVoiceEntryPoint(entry_point, prefer_voice) &&
      entry_point != AssistantEntryPoint::kHotword) {
    should_attempt_warmer_welcome_ = false;
    StartVoiceInteraction();
    return;
  }

  if (entry_point == AssistantEntryPoint::kProactiveSuggestions) {
    should_attempt_warmer_welcome_ = false;
    // When entering Assistant with a proactive suggestions interaction, there
    // will be no server latency as the response for the interaction has already
    // been cached on the client. To avoid jank, we need to post a task to start
    // our interaction to give the Assistant UI a chance to initialize itself.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantInteractionController::
                           StartProactiveSuggestionsInteraction,
                       weak_factory_.GetWeakPtr(),
                       assistant_controller_->suggestions_controller()
                           ->model()
                           ->GetProactiveSuggestions()));
    return;
  }

  if (entry_point == AssistantEntryPoint::kStylus) {
    should_attempt_warmer_welcome_ = false;
    // When the embedded Assistant feature is enabled, we call ShowUi(kStylus)
    // OnHighlighterSelectionRecognized. But we are not actually using stylus.
    if (!app_list_features::IsAssistantLauncherUIEnabled())
      model_.SetInputModality(InputModality::kStylus);
    return;
  }

  if (!chromeos::assistant::features::IsWarmerWelcomeEnabled())
    return;

  should_attempt_warmer_welcome_ =
      should_attempt_warmer_welcome_ &&
      assistant::util::ShouldAttemptWarmerWelcome(entry_point);

  // Explicitly check the interaction state to ensure warmer welcome will
  // not interrupt any ongoing active interactions. This happens, for example,
  // when the first Assistant launch of the current user session is trigger by
  // Assistant notification, or directly sending query without showing Ui
  // during integration test.
  if (model_.interaction_state() == InteractionState::kActive)
    should_attempt_warmer_welcome_ = false;

  if (!should_attempt_warmer_welcome_)
    return;

  // TODO(yileili): Currently WW is only triggered when the first Assistant
  // launch of the user session does not automatically start an interaction that
  // would otherwise cause us to interrupt the user. Need further UX design to
  // attempt WW after the first interaction.
  auto* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  DCHECK(pref_service);

  auto num_warmer_welcome_triggered =
      pref_service->GetInteger(prefs::kAssistantNumWarmerWelcomeTriggered);
  if (num_warmer_welcome_triggered < kWarmerWelcomesMaxTimesTriggered) {
    // If the user has opted to launch Assistant with the mic open, we can
    // reasonably assume there is an expectation of TTS.
    assistant_->StartWarmerWelcomeInteraction(
        num_warmer_welcome_triggered,
        /*allow_tts=*/launch_with_mic_open);
    pref_service->SetInteger(prefs::kAssistantNumWarmerWelcomeTriggered,
                             ++num_warmer_welcome_triggered);
  }
  should_attempt_warmer_welcome_ = false;
}

void AssistantInteractionController::StartMetalayerInteraction(
    const gfx::Rect& region) {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
      l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN),
      AssistantQuerySource::kStylus));

  assistant_->StartMetalayerInteraction(region);
}

void AssistantInteractionController::StartProactiveSuggestionsInteraction(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  // For a proactive suggestions interaction, we've already cached the response
  // but we still need to spoof lifecycle events. This is only safe to do if we
  // aren't already in the midst of an interaction.
  DCHECK_EQ(InteractionState::kInactive, model_.interaction_state());

  // To be extra protective of interaction lifecycle when DCHECK is disabled,
  // we'll ignore any attempts to start a proactive suggestions interaction if
  // an interaction is already in progress.
  if (model_.interaction_state() != InteractionState::kInactive)
    return;

  const std::string& description = proactive_suggestions->description();
  const std::string& search_query = proactive_suggestions->search_query();

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
      description, AssistantQuerySource::kProactiveSuggestions));

  OnInteractionStarted(AssistantInteractionMetadata::New(
      AssistantInteractionType::kText,
      AssistantQuerySource::kProactiveSuggestions, /*query=*/description));

  OnHtmlResponse(proactive_suggestions->html(), /*fallback=*/std::string());

  // TODO(dmblack): Support suggestion chips from the server when available.
  if (!search_query.empty()) {
    std::vector<AssistantSuggestionPtr> suggestions;
    suggestions.push_back(CreateSearchSuggestion(search_query));
    OnSuggestionsResponse(std::move(suggestions));
  }

  OnInteractionFinished(AssistantInteractionResolution::kNormal);
}

void AssistantInteractionController::StartScreenContextInteraction(
    AssistantQuerySource query_source) {
  StopActiveInteraction(false);

  model_.SetPendingQuery(std::make_unique<AssistantTextQuery>(
      l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN),
      query_source));

  // Note that screen context was cached when the UI was launched.
  assistant_->StartCachedScreenContextInteraction();
}

void AssistantInteractionController::StartTextInteraction(
    const std::string text,
    bool allow_tts,
    AssistantQuerySource query_source) {
  StopActiveInteraction(false);

  model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text, query_source));

  assistant_->StartTextInteraction(text, query_source, allow_tts);
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
