// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller_impl.h"

#include <optional>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_text_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/android_intent_helper.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

namespace {

using assistant::features::IsWaitSchedulingEnabled;

// Android.
constexpr char kAndroidIntentScheme[] = "intent://";
constexpr char kAndroidIntentPrefix[] = "#Intent";

// Helpers ---------------------------------------------------------------------

// Returns true if device is in tablet mode, false otherwise.
bool IsTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

bool launch_with_mic_open() {
  return AssistantState::Get()->launch_with_mic_open().value_or(false);
}

// Returns whether the Assistant UI should open in voice mode by default.
// Note that this can be overruled by the entry-point (for example using hotword
// will always open in voice mode).
bool IsPreferVoice() {
  return launch_with_mic_open() || IsTabletMode();
}

PrefService* pref_service() {
  auto* result =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(result);
  return result;
}

}  // namespace

// AssistantInteractionController ----------------------------------------------

AssistantInteractionControllerImpl::AssistantInteractionControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);

  assistant_controller_observation_.Observe(AssistantController::Get());
  display_observation_.Observe(display::Screen::GetScreen());
}

AssistantInteractionControllerImpl::~AssistantInteractionControllerImpl() {
  model_.RemoveObserver(this);
  if (assistant_)
    assistant_->RemoveAssistantInteractionSubscriber(this);
}

// static
void AssistantInteractionControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kAssistantTimeOfLastInteraction,
                             base::Time());
}

void AssistantInteractionControllerImpl::SetAssistant(
    assistant::Assistant* assistant) {
  if (assistant_)
    assistant_->RemoveAssistantInteractionSubscriber(this);

  assistant_ = assistant;

  if (assistant_)
    assistant_->AddAssistantInteractionSubscriber(this);
}

const AssistantInteractionModel* AssistantInteractionControllerImpl::GetModel()
    const {
  return &model_;
}

base::TimeDelta
AssistantInteractionControllerImpl::GetTimeDeltaSinceLastInteraction() const {
  return base::Time::Now() -
         pref_service()->GetTime(prefs::kAssistantTimeOfLastInteraction);
}

bool AssistantInteractionControllerImpl::HasHadInteraction() const {
  return has_had_interaction_;
}

void AssistantInteractionControllerImpl::StartTextInteraction(
    const std::string& text,
    bool allow_tts,
    AssistantQuerySource query_source) {
  DCHECK(assistant_);

  model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text, query_source));

  assistant_->StartTextInteraction(text, query_source, allow_tts);
}

void AssistantInteractionControllerImpl::OnAssistantControllerConstructed() {
  AssistantUiController::Get()->GetModel()->AddObserver(this);
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantInteractionControllerImpl::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

void AssistantInteractionControllerImpl::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type == DeepLinkType::kReminders) {
    using ReminderAction = assistant::util::ReminderAction;
    const std::optional<ReminderAction>& action =
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
        const std::optional<std::string>& client_id =
            GetDeepLinkParam(params, DeepLinkParam::kClientId);
        if (client_id && !client_id.value().empty()) {
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

  const std::optional<std::string>& query =
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

  const AssistantEntryPoint entry_point =
      GetDeepLinkParamAsEntryPoint(params, DeepLinkParam::kEntryPoint)
          .value_or(AssistantEntryPoint::kDeepLink);

  // Explicitly call ShowUi() to set the correct Assistant entry point.
  // NOTE: ShowUi() will no-op if UI is already shown.
  AssistantUiController::Get()->ShowUi(entry_point);

  const AssistantQuerySource query_source =
      GetDeepLinkParamAsQuerySource(params, DeepLinkParam::kQuerySource)
          .value_or(AssistantQuerySource::kDeepLink);

  // A text query originating from a deep link will carry forward the allowance/
  // forbiddance of TTS from the previous response. This is predominately aimed
  // at addressing the use case of tapping a card from a previous query response
  // in which case we are essentially continuing the preceding interaction. Deep
  // links are also potentially fired from notifications or other sources. If we
  // need to allow deep link creators the ability to set |allow_tts| explicitly,
  // we can expose a deep link parameter when the need arises.
  StartTextInteraction(query.value(), /*allow_tts=*/model_.response() &&
                                          model_.response()->has_tts(),
                       query_source);
}

void AssistantInteractionControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  switch (new_visibility) {
    case AssistantVisibility::kClosed:
      // When the UI is closed we need to stop any active interaction. We also
      // reset the interaction state and restore the default input modality.
      StopActiveInteraction(true);
      model_.ClearInteraction();
      model_.SetInputModality(GetDefaultInputModality());
      break;
    case AssistantVisibility::kVisible:
      OnUiVisible(entry_point.value());
      break;
    case AssistantVisibility::kClosing:
      break;
  }
}

void AssistantInteractionControllerImpl::OnInputModalityChanged(
    InputModality input_modality) {
  if (!IsVisible())
    return;

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

void AssistantInteractionControllerImpl::OnMicStateChanged(MicState mic_state) {
  // We should stop ChromeVox from speaking when opening the mic.
  if (mic_state == MicState::kOpen)
    Shell::Get()->accessibility_controller()->SilenceSpokenFeedback();
}

void AssistantInteractionControllerImpl::OnCommittedQueryChanged(
    const AssistantQuery& assistant_query) {
  // Update the time of the last Assistant interaction so that we can later
  // determine how long it has been since a user interacted with the Assistant.
  // NOTE: We do this in OnCommittedQueryChanged() to filter out accidental
  // interactions that would still have triggered OnInteractionStarted().
  pref_service()->SetTime(prefs::kAssistantTimeOfLastInteraction,
                          base::Time::Now());

  // Cache the fact that the user has now had an interaction with the Assistant
  // during this user session.
  has_had_interaction_ = true;

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
      AssistantUiController::Get()->GetModel()->entry_point());
  assistant::util::RecordAssistantQuerySource(assistant_query.source());
}

// TODO(b/140565663): Set pending query from |metadata| and remove calls to set
// pending query that occur outside of this method.
void AssistantInteractionControllerImpl::OnInteractionStarted(
    const AssistantInteractionMetadata& metadata) {
  VLOG(1) << __func__;

  // Stop the interaction if the opt-in window is active.
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (assistant_setup && assistant_setup->BounceOptInWindowIfActive()) {
    StopActiveInteraction(true);
    return;
  }

  const bool is_voice_interaction =
      assistant::AssistantInteractionType::kVoice == metadata.type;

  if (is_voice_interaction) {
    // If the Assistant UI is not visible yet, and |is_voice_interaction| is
    // true, then it will be sure that Assistant is fired via OKG. ShowUi will
    // not update the Assistant entry point if the UI is already visible.
    AssistantUiController::Get()->ShowUi(AssistantEntryPoint::kHotword);
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
          metadata.query, metadata.source));
    }
    model_.CommitPendingQuery();
    model_.SetMicState(MicState::kClosed);
  }

  // Start caching a new Assistant response for the interaction.
  model_.SetPendingResponse(base::MakeRefCounted<AssistantResponse>());
}

void AssistantInteractionControllerImpl::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  VLOG(1) << __func__;

  base::UmaHistogramEnumeration("Assistant.Interaction.Resolution", resolution);
  model_.SetMicState(MicState::kClosed);

  // If we don't have an active interaction, that indicates that this
  // interaction was explicitly stopped outside of LibAssistant. In this case,
  // we ensure that the mic is closed but otherwise ignore this event.
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  model_.SetInteractionState(InteractionState::kInactive);

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
  // this is a prerequisite step to being able to commit the pending response.
  if (model_.pending_query().type() != AssistantQueryType::kNull)
    model_.CommitPendingQuery();

  AssistantResponse* response = GetResponseForActiveInteraction();

  // Some interaction resolutions require special handling.
  switch (resolution) {
    case AssistantInteractionResolution::kError: {
      // In the case of error, we show an appropriate message to the user. Do
      // not show another error if an identical one already exists in the
      // response.
      auto err = std::make_unique<AssistantErrorElement>(
          IDS_ASH_ASSISTANT_ERROR_GENERIC);

      if (!response->ContainsUiElement(err.get()))
        response->AddUiElement(std::move(err));

      break;
    }
    case AssistantInteractionResolution::kMultiDeviceHotwordLoss:
      // In the case of hotword loss to another device, we show an appropriate
      // message to the user.
      response->AddUiElement(
          std::make_unique<AssistantTextElement>(l10n_util::GetStringUTF8(
              IDS_ASH_ASSISTANT_MULTI_DEVICE_HOTWORD_LOSS)));
      break;
    case AssistantInteractionResolution::kMicTimeout:
      // Interactions resolving due to mic timeout are already handled above
      // outside the switch.
      NOTREACHED();
    case AssistantInteractionResolution::kInterruption:  // fallthrough
    case AssistantInteractionResolution::kNormal:
      // No special handling required.
      break;
  }

  // If |response| is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (response == model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnHtmlResponse(
    const std::string& html,
    const std::string& fallback) {
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  DCHECK(AssistantUiController::Get());
  AssistantResponse* response = GetResponseForActiveInteraction();
  response->AddUiElement(std::make_unique<AssistantCardElement>(
      html, fallback,
      AssistantUiController::Get()->GetModel()->AppListBubbleWidth()));

  // If |response| is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (response == model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnSuggestionPressed(
    const base::UnguessableToken& suggestion_id) {
  // There are two potential data model that provide suggestions. One is the
  // AssistantSuggestionModel which provides the zero state suggestions, the
  // other is AssistantResponse which provider server generated suggestions
  // based on current query.
  auto* suggestion =
      AssistantSuggestionsController::Get()->GetModel()->GetSuggestionById(
          suggestion_id);
  if (!suggestion && model_.response())
    suggestion = model_.response()->GetSuggestionById(suggestion_id);

  DCHECK(suggestion);

  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    // Note that we post a new task when opening the |action_url| associated
    // with |suggestion| as this will potentially cause Assistant UI to close
    // and destroy |suggestion| in the process. Failure to post in this case
    // would cause any subsequent observers of this suggestion chip event to
    // receive a deleted pointer.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantController::OpenUrl,
                       AssistantController::Get()->GetWeakPtr(),
                       suggestion->action_url, /*in_background=*/false,
                       /*from_server=*/false));
    return;
  }

  // Determine query source from suggestion type.
  AssistantQuerySource query_source;
  switch (suggestion->type) {
    case AssistantSuggestionType::kBetterOnboarding:
      // There should be no more `AssistantSuggestionType::KBetterOnboarding` as
      // the ui gets removed. Leave the code as is.
      query_source = AssistantQuerySource::kBetterOnboarding;
      break;
    case AssistantSuggestionType::kConversationStarter:
      query_source = AssistantQuerySource::kConversationStarter;
      break;
    case AssistantSuggestionType::kUnspecified:
      query_source = AssistantQuerySource::kSuggestionChip;
      break;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  // Note that a text query originating from a suggestion chip will carry
  // forward the allowance/forbiddance of TTS from the previous response. This
  // is because suggestion chips pressed after a voice query should continue to
  // return TTS, as really the text interaction is just a continuation of the
  // user's preceding voice interaction.
  StartTextInteraction(
      suggestion->text,
      /*allow_tts=*/model_.response() && model_.response()->has_tts(),
      query_source);
}

void AssistantInteractionControllerImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  // Ignore the state in the process of changing the tablet state.
  if (display::IsTabletStateChanging(state)) {
    return;
  }

  // The default input modality is different for tablet and normal mode.
  // Change input modality to the new default input modality.
  if (!HasActiveInteraction() && !IsVisible())
    model_.SetInputModality(GetDefaultInputModality());
}

void AssistantInteractionControllerImpl::OnSuggestionsResponse(
    const std::vector<AssistantSuggestion>& suggestions) {
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  AssistantResponse* response = GetResponseForActiveInteraction();
  response->AddSuggestions(suggestions);

  // If |response| is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (response == model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnTextResponse(
    const std::string& text) {
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  AssistantResponse* response = GetResponseForActiveInteraction();
  response->AddUiElement(std::make_unique<AssistantTextElement>(text));

  // If |response| is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (response == model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnSpeechRecognitionStarted() {}

void AssistantInteractionControllerImpl::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>(
      high_confidence_text, low_confidence_text));
}

void AssistantInteractionControllerImpl::OnSpeechRecognitionEndOfUtterance() {
  model_.SetMicState(MicState::kClosed);
}

void AssistantInteractionControllerImpl::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  // We sometimes receive this event with an empty payload when the interaction
  // is resolving due to mic timeout. In such cases, we should not commit the
  // pending query as the interaction will be discarded.
  if (final_result.empty())
    return;

  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>(final_result));
  model_.CommitPendingQuery();
}

void AssistantInteractionControllerImpl::OnSpeechLevelUpdated(
    float speech_level) {
  model_.SetSpeechLevel(speech_level);
}

void AssistantInteractionControllerImpl::OnTtsStarted(bool due_to_error) {
  // When Assistant is talking, ChromeVox should not be.
  Shell::Get()->accessibility_controller()->SilenceSpokenFeedback();

  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  // Commit the pending query in whatever state it's in. In most cases the
  // pending query is already committed, but we must always commit the pending
  // query before committing a pending response.
  if (model_.pending_query().type() != AssistantQueryType::kNull)
    model_.CommitPendingQuery();

  AssistantResponse* response = GetResponseForActiveInteraction();

  if (due_to_error) {
    // In the case of an error occurring during a voice interaction, this is our
    // earliest indication that the mic has closed.
    model_.SetMicState(MicState::kClosed);

    // Create an error and add it to response. Do not add it if another
    // identical error already exists in response.
    auto err = std::make_unique<AssistantErrorElement>(
        IDS_ASH_ASSISTANT_ERROR_GENERIC);

    if (!response->ContainsUiElement(err.get()))
      response->AddUiElement(std::move(err));
  }

  response->set_has_tts(true);

  // If |response| is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (response == model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnWaitStarted() {
  DCHECK(IsWaitSchedulingEnabled());
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  // If necessary, commit the pending query in whatever state it's in. This is
  // prerequisite to being able to commit a response.
  if (model_.pending_query().type() != AssistantQueryType::kNull)
    model_.CommitPendingQuery();

  // If our response is pending, commit it to cause the response for the
  // previous interaction, if one exists, to be animated off stage and the new
  // |response| to begin rendering.
  if (model_.pending_response())
    model_.CommitPendingResponse();
}

void AssistantInteractionControllerImpl::OnOpenUrlResponse(const GURL& url,
                                                           bool in_background) {
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  // We need to indicate that the navigation attempt is occurring as a result of
  // a server response so that we can differentiate from navigation attempts
  // initiated by direct user interaction.
  AssistantController::Get()->OpenUrl(url, in_background, /*from_server=*/true);
}

void AssistantInteractionControllerImpl::OnOpenAppResponse(
    const assistant::AndroidAppInfo& app_info) {
  if (!HasActiveInteraction()) {
    DVLOG(1) << "Assistant: Dropping response outside of active interaction";
    return;
  }

  auto* android_helper = AndroidIntentHelper::GetInstance();
  if (!android_helper)
    return;

  auto intent = android_helper->GetAndroidAppLaunchIntent(app_info);
  if (!intent.has_value())
    return;

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
  AssistantController::Get()->OpenUrl(GURL(intent_str), /*in_background=*/false,
                                      /*from_server=*/true);
}

void AssistantInteractionControllerImpl::OnDialogPlateButtonPressed(
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

void AssistantInteractionControllerImpl::OnDialogPlateContentsCommitted(
    const std::string& text) {
  DCHECK(!text.empty());
  StartTextInteraction(
      text, /*allow_tts=*/false,
      /*query_source=*/AssistantQuerySource::kDialogPlateTextField);
}

bool AssistantInteractionControllerImpl::HasActiveInteraction() const {
  return model_.interaction_state() == InteractionState::kActive;
}

void AssistantInteractionControllerImpl::OnUiVisible(
    AssistantEntryPoint entry_point) {
  DCHECK(IsVisible());

  // We don't explicitly start a new voice interaction if the entry point
  // is hotword since in such cases a voice interaction will already be in
  // progress.
  if (assistant::util::IsVoiceEntryPoint(entry_point, IsPreferVoice()) &&
      entry_point != AssistantEntryPoint::kHotword) {
    StartVoiceInteraction();
    return;
  }
}

void AssistantInteractionControllerImpl::StartVoiceInteraction() {
  model_.SetPendingQuery(std::make_unique<AssistantVoiceQuery>());

  assistant_->StartVoiceInteraction();
}

void AssistantInteractionControllerImpl::StopActiveInteraction(
    bool cancel_conversation) {
  // Even though the interaction state will be asynchronously set to inactive
  // via a call to OnInteractionFinished(Resolution), we explicitly set it to
  // inactive here to prevent processing any additional UI related service
  // events belonging to the interaction being stopped.
  model_.SetInteractionState(InteractionState::kInactive);
  model_.ClearPendingQuery();

  if (AssistantState::Get()->assistant_status() ==
      assistant::AssistantStatus::READY) {
    assistant_->StopActiveInteraction(cancel_conversation);
  }

  // Because we are stopping an interaction in progress, we discard any pending
  // response for it that is cached to prevent it from being committed when the
  // interaction is finished.
  model_.ClearPendingResponse();
}

InputModality AssistantInteractionControllerImpl::GetDefaultInputModality()
    const {
  return IsPreferVoice() ? InputModality::kVoice : InputModality::kKeyboard;
}

AssistantResponse*
AssistantInteractionControllerImpl::GetResponseForActiveInteraction() {
  // Returns the response for the active interaction. In response processing v2,
  // this may be the pending response (if no client ops have yet been received)
  // or else is the committed response.
  return model_.pending_response() ? model_.pending_response()
                                   : model_.response();
}

AssistantVisibility AssistantInteractionControllerImpl::GetVisibility() const {
  return AssistantUiController::Get()->GetModel()->visibility();
}

bool AssistantInteractionControllerImpl::IsVisible() const {
  return GetVisibility() == AssistantVisibility::kVisible;
}

}  // namespace ash
