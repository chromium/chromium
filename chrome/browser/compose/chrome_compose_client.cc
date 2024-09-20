// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/third_party/icu/icu_utf.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_text_usage_logger.h"
#include "chrome/browser/compose/proactive_nudge_tracker.h"
#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/common/compose/type_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

std::u16string RemoveLastCharIfInvalid(std::u16string str) {
  // TODO(b/323902463): Have Autofill send a valid string, i.e. truncated to a
  // valid grapheme, in FormFieldData.selected_text to ensure greatest
  // preservation of the original selected text.
  if (!str.empty() && CBU16_IS_LEAD(str.back())) {
    str.pop_back();
  }
  return str;
}

bool ComposeNudgeShowStatusDisabledByConfig(compose::ComposeShowStatus status) {
  switch (status) {
    case compose::ComposeShowStatus::
        kProactiveNudgeDisabledGloballyByUserPreference:
    case compose::ComposeShowStatus::
        kProactiveNudgeDisabledForSiteByUserPreference:
    case compose::ComposeShowStatus::kProactiveNudgeFeatureDisabled:
    case compose::ComposeShowStatus::kProactiveNudgeDisabledByMSBB:
    case compose::ComposeShowStatus::
        kProactiveNudgeBlockedBySegmentationPlatform:
      return true;
    default:
      return false;
  }
}

}  // namespace

// ChromeComposeClient::FieldChangeObserver
ChromeComposeClient::FieldChangeObserver::FieldChangeObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
}

ChromeComposeClient::FieldChangeObserver::~FieldChangeObserver() = default;

void ChromeComposeClient::FieldChangeObserver::OnSuggestionsShown(
    autofill::AutofillManager& manager) {
  text_field_change_event_count_ = 0;
}

void ChromeComposeClient::FieldChangeObserver::OnAfterTextFieldDidChange(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    const std::u16string& text_value) {
  ++text_field_change_event_count_;
  if (text_field_change_event_count_ >=
      compose::GetComposeConfig().nudge_field_change_event_max) {
    HideComposeNudges();
    text_field_change_event_count_ = 0;
  }
}

void ChromeComposeClient::FieldChangeObserver::HideComposeNudges() {
  if (autofill::AutofillClient* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents_)) {
    // Only hide open suggestions if they are of compose type.
    base::span<const autofill::Suggestion> suggestions =
        autofill_client->GetAutofillSuggestions();
    if ((suggestions.size() == 1 &&
         autofill::GetFillingProductFromSuggestionType(suggestions[0].type) ==
             autofill::FillingProduct::kCompose) ||
        skip_suggestion_type_for_test_) {
      autofill_client->HideAutofillSuggestions(
          autofill::SuggestionHidingReason::kFieldValueChanged);
    }
  }
}

void ChromeComposeClient::FieldChangeObserver::SetSkipSuggestionTypeForTest(
    bool skip_suggestion_type) {
  skip_suggestion_type_for_test_ = skip_suggestion_type;
}

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      profile_(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())),
      nudge_tracker_(segmentation_platform::SegmentationPlatformServiceFactory::
                         GetForProfile(profile_),
                     this),
      field_change_observer_(web_contents) {
  auto ukm_source_id =
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId();
  page_ukm_tracker_ = std::make_unique<compose::PageUkmTracker>(ukm_source_id);
  opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  pref_service_ = profile_->GetPrefs();
  proactive_nudge_enabled_.Init(prefs::kEnableProactiveNudge, pref_service_);

  compose_enabling_ = std::make_unique<ComposeEnabling>(
      profile_, IdentityManagerFactory::GetForProfileIfExists(profile_),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_));

  if (GetOptimizationGuide()) {
    std::vector<optimization_guide::proto::OptimizationType> types;
    if (compose_enabling_->IsEnabled().has_value()) {
      types.push_back(optimization_guide::proto::OptimizationType::COMPOSE);
    }

    if (!types.empty()) {
      GetOptimizationGuide()->RegisterOptimizationTypes(types);
    }
  }

  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
  nudge_tracker_.StartObserving(web_contents);
}

ChromeComposeClient::~ChromeComposeClient() {
  // Sessions may call back during destruction through ComposeSession::Observer.
  // Let's ensure that happens before destroying anything else.
  sessions_.clear();
  debug_session_.reset();
}

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeClientUntrustedPageHandler>
        client_handler,
    mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
        handler,
    mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog) {
  client_page_receiver_.reset();
  client_page_receiver_.Bind(std::move(client_handler));

  url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (origin ==
      url::Origin::Create(GURL(chrome::kChromeUIUntrustedComposeUrl))) {
    debug_session_ = std::make_unique<ComposeSession>(
        &GetWebContents(), GetModelExecutor(), GetSessionId(),
        GetInnerTextProvider(),
        autofill::FieldGlobalId{{}, autofill::FieldRendererId(-1)},
        IsPageLanguageSupported(), this);
    debug_session_->set_collect_inner_text(false);
    debug_session_->set_fre_complete(
        pref_service_->GetBoolean(prefs::kPrefHasCompletedComposeFRE));
    debug_session_->set_current_msbb_state(GetMSBBStateFromPrefs());
    debug_session_->Bind(std::move(handler), std::move(dialog));
    return;
  }

  std::optional<FieldIdentifier> target_field;
  if (skip_show_dialog_for_test_) {
    target_field = active_compose_ids_;
  } else if (compose_dialog_controller_) {
    target_field = compose_dialog_controller_->GetFieldIds();
  }
  if (!target_field.has_value()) {
    DLOG(WARNING)
        << "Unable to bind dialog because no controller is available.";
    compose_dialog_controller_.reset();
    return;
  }
  if (!HasSession(target_field->first)) {
    DLOG(WARNING) << "Unable to bind dialog because there is no session for "
                     "the underlying field.";
    compose_dialog_controller_.reset();
    return;
  }
  active_compose_ids_ = target_field;
  sessions_.at(active_compose_ids_.value().first)
      ->Bind(std::move(handler), std::move(dialog));
}

void ChromeComposeClient::ShowComposeDialog(
    EntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeCallback callback) {
  active_compose_ids_ = std::make_optional<FieldIdentifier>(
      trigger_field.global_id(), trigger_field.renderer_form_id());

  // The selected text received from Autofill is a UTF-16 string truncated using
  // substr, which will result in a rendered invalid character in the Compose
  // dialog if it splits a surrogate pair character. Ensure that any invalid
  // characters are removed.
  std::string selected_text =
      base::UTF16ToUTF8(RemoveLastCharIfInvalid(trigger_field.selected_text()));

  // We only want to resume if there is an existing, unexpired session and the
  // popup was clicked or the selection is empty. If the context menu is clicked
  // with a selection we start a new session using the selection.
  bool popup_clicked = ui_entry_point == EntryPoint::kAutofillPopup;
  bool resume_current_session = ActiveFieldHasUnexpiredSession() &&
                                (popup_clicked || selected_text.empty());

  if (resume_current_session) {
    PrepareToResumeExistingSession(std::move(callback),
                                   /*has_selection=*/!selected_text.empty(),
                                   popup_clicked);
  } else {
    CreateNewSession(std::move(callback), trigger_field, selected_text,
                     popup_clicked);
  }
  last_popup_trigger_source_ =
      autofill::AutofillSuggestionTriggerSource::kUnspecified;

  if (!skip_show_dialog_for_test_) {
    // The bounds given by autofill are relative to the top level frame. Here we
    // offset by the WebContents container to make up for that.
    gfx::RectF bounds_in_screen = trigger_field.bounds();
    bounds_in_screen.Offset(
        GetWebContents().GetContainerBounds().OffsetFromOrigin());

    show_dialog_start_ = base::TimeTicks::Now();
    DCHECK(active_compose_ids_.has_value());
    compose_dialog_controller_ = chrome::ShowComposeDialog(
        GetWebContents(), bounds_in_screen, active_compose_ids_.value());
  }
}

bool ChromeComposeClient::HasSession(
    const autofill::FieldGlobalId& trigger_field_id) {
  auto it = sessions_.find(trigger_field_id);
  return it != sessions_.end();
}

void ChromeComposeClient::ShowUI() {
  if (compose_dialog_controller_) {
    compose_dialog_controller_->ShowUI(
        base::BindOnce(&ChromeComposeClient::ShowSavedStateNotification,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*field_id=*/active_compose_ids_->first));
    compose::LogComposeDialogOpenLatency(base::TimeTicks::Now() -
                                         show_dialog_start_);
  }
}

void ChromeComposeClient::CloseUI(compose::mojom::CloseReason reason) {
  switch (reason) {
    case compose::mojom::CloseReason::kFirstRunCloseButton:
      SetFirstRunSessionCloseReason(
          compose::ComposeFreOrMsbbSessionCloseReason::kCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kMSBBCloseButton:
      SetMSBBSessionCloseReason(
          compose::ComposeFreOrMsbbSessionCloseReason::kCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kCloseButton:
      base::RecordAction(
          base::UserMetricsAction("Compose.EndedSession.CloseButtonClicked"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kCloseButtonPressed);
      LaunchHatsSurveyForActiveSession(
          compose::ComposeSessionCloseReason::kCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kInsertButton:
      base::RecordAction(
          base::UserMetricsAction("Compose.EndedSession.InsertButtonClicked"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kInsertedResponse);
      SetMSBBSessionCloseReason(compose::ComposeFreOrMsbbSessionCloseReason::
                                    kAckedOrAcceptedWithInsert);
      LaunchHatsSurveyForActiveSession(
          compose::ComposeSessionCloseReason::kInsertedResponse);
      SetFirstRunSessionCloseReason(
          compose::ComposeFreOrMsbbSessionCloseReason::
              kAckedOrAcceptedWithInsert);
      page_ukm_tracker_->ComposeTextInserted();
      break;
  }

  RemoveActiveSession();

  if (compose_dialog_controller_) {
    compose_dialog_controller_->Close();
  }
}

void ChromeComposeClient::CompleteFirstRun() {
  pref_service_->SetBoolean(prefs::kPrefHasCompletedComposeFRE, true);

  // This marks the end of the FRE "session" as the dialog moves to the main UI
  // state. Mark all existing sessions as having completed the FRE and log
  // relevant metrics.
  UpdateAllSessionsWithFirstRunComplete();
  open_settings_requested_ = false;
  SetFirstRunSessionCloseReason(compose::ComposeFreOrMsbbSessionCloseReason::
                                    kAckedOrAcceptedWithoutInsert);
}

void ChromeComposeClient::OpenComposeSettings() {
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  // `browser` should never be null here. This can only be triggered when there
  // is an active ComposeSession, which  is indirectly owned by the same
  // WebContents that holds the field that the Compose dialog is triggered from.
  // The session is created when that dialog is opened and it is destroyed if
  // its WebContents is destroyed.
  CHECK(browser);

  ShowPromoInPage::Params params;
  params.target_url = chrome::GetSettingsUrl(chrome::kSyncSetupSubPage);
  params.bubble_anchor_id = kAnonymizedUrlCollectionPersonalizationSettingId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomRight;
  params.bubble_text =
      l10n_util::GetStringUTF16(IDS_COMPOSE_MSBB_IPH_BUBBLE_TEXT);
  params.close_button_alt_text_id =
      IDS_COMPOSE_MSBB_IPH_BUBBLE_CLOSE_BUTTON_LABEL_TEXT;

  ComposeSession* active_session = GetSessionForActiveComposeField();
  if (active_session) {
    active_session->set_msbb_settings_opened();
  }

  base::RecordAction(
      base::UserMetricsAction("Compose.SessionPaused.MSBBSettingsShown"));
  ShowPromoInPage::Start(browser, std::move(params));

  open_settings_requested_ = true;
}

void ChromeComposeClient::GetInnerText(
    content::RenderFrameHost& host,
    std::optional<int> node_id,
    content_extraction::InnerTextCallback callback) {
  content_extraction::GetInnerText(host, node_id, std::move(callback));
}

void ChromeComposeClient::UpdateAllSessionsWithFirstRunComplete() {
  if (debug_session_) {
    debug_session_->SetFirstRunCompleted();
  }
  for (const auto& session : sessions_) {
    session.second->SetFirstRunCompleted();
  }
}

void ChromeComposeClient::PrepareToResumeExistingSession(
    ComposeCallback callback,
    bool has_selection,
    bool popup_clicked) {
  ComposeSession* current_session = GetSessionForActiveComposeField();
  CHECK(current_session);
  current_session->set_compose_callback(std::move(callback));
  // Update the msbb state which can change while the session is hidden.
  current_session->set_current_msbb_state(GetMSBBStateFromPrefs());
  current_session->MaybeRefreshPageContext(has_selection);

  if (popup_clicked) {
    if (last_popup_trigger_source_ ==
        autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus) {
      compose::LogResumeSessionEntryPoint(
          compose::ComposeEntryPoint::kSavedStateNotification);
    } else {
      compose::LogResumeSessionEntryPoint(
          compose::ComposeEntryPoint::kSavedStateNudge);
    }
  } else {
    compose::LogResumeSessionEntryPoint(
        compose::ComposeEntryPoint::kContextMenu);
  }
}

void ChromeComposeClient::CreateNewSession(
    ComposeCallback callback,
    const autofill::FormFieldData& trigger_field,
    std::string_view selected_text,
    bool popup_clicked) {
  ComposeSession* current_session;
  autofill::FieldGlobalId trigger_field_id = active_compose_ids_.value().first;
  if (HasSession(trigger_field_id)) {
    current_session = sessions_.at(trigger_field_id).get();

    // Set the final state for the existing session which will be closed to
    // start a new one.
    compose::ComposeFreOrMsbbSessionCloseReason fre_or_msbb_close_reason;
    if (current_session->HasExpired()) {
      base::RecordAction(
          base::UserMetricsAction("Compose.EndedSession.EndedImplicitly"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kExceededMaxDuration);
      fre_or_msbb_close_reason =
          compose::ComposeFreOrMsbbSessionCloseReason::kExceededMaxDuration;
    } else {
      base::RecordAction(base::UserMetricsAction(
          "Compose.EndedSession.NewSessionWithSelectedText"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kReplacedWithNewSession);
      fre_or_msbb_close_reason =
          compose::ComposeFreOrMsbbSessionCloseReason::kReplacedWithNewSession;
    }
    // If the existing session has not accepted consent then set the equivalent
    // close reason here. If consent was accepted in this session the close
    // reason will remain as |kAckedOrAcceptedWithoutInsert|.
    if (!current_session->get_fre_complete()) {
      SetFirstRunSessionCloseReason(fre_or_msbb_close_reason);
    }
    if (!current_session->get_current_msbb_state()) {
      SetMSBBSessionCloseReason(fre_or_msbb_close_reason);
    }
  }

  auto new_session = std::make_unique<ComposeSession>(
      &GetWebContents(), GetModelExecutor(), GetSessionId(),
      GetInnerTextProvider(), trigger_field.global_id(),
      IsPageLanguageSupported(), this, std::move(callback));
  current_session = new_session.get();
  sessions_.insert_or_assign(active_compose_ids_.value().first,
                             std::move(new_session));

  // Set the FRE state of the new session.
  auto fre_state =
      pref_service_->GetBoolean(prefs::kPrefHasCompletedComposeFRE);
  current_session->set_fre_complete(fre_state);

  // Set the MSBB state of the new session.
  current_session->set_current_msbb_state(GetMSBBStateFromPrefs());

  current_session->InitializeWithText(selected_text);

  // Record the UI state that new sessions are created in.
  if (!fre_state) {
    base::RecordAction(
        base::UserMetricsAction("Compose.DialogSeen.FirstRunDisclaimer"));
  } else if (!GetMSBBStateFromPrefs()) {
    base::RecordAction(
        base::UserMetricsAction("Compose.DialogSeen.FirstRunMSBB"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Compose.DialogSeen.MainDialog"));
  }

  // Only record the selection length for new sessions.
  auto utf8_chars = base::CountUnicodeCharacters(selected_text);
  compose::LogComposeDialogSelectionLength(
      utf8_chars.has_value() ? utf8_chars.value() : 0);

  if (popup_clicked) {
    switch (most_recent_nudge_entry_point_) {
      case compose::ComposeEntryPoint::kProactiveNudge:
        current_session->set_started_with_proactive_nudge();
        page_ukm_tracker_->ProactiveNudgeOpened();
        compose::LogComposeProactiveNudgeCtr(
            compose::ComposeNudgeCtrEvent::kDialogOpened);
        compose::LogStartSessionEntryPoint(
            compose::ComposeEntryPoint::kProactiveNudge);
        break;
      case compose::ComposeEntryPoint::kSelectionNudge:
        compose::LogComposeSelectionNudgeCtr(
            compose::ComposeNudgeCtrEvent::kDialogOpened);
        compose::LogStartSessionEntryPoint(
            compose::ComposeEntryPoint::kSelectionNudge);
        break;
      case compose::ComposeEntryPoint::kContextMenu:
      case compose::ComposeEntryPoint::kSavedStateNudge:
      case compose::ComposeEntryPoint::kSavedStateNotification:
        break;
    }
  } else {
    compose::LogStartSessionEntryPoint(
        compose::ComposeEntryPoint::kContextMenu);
  }
}

void ChromeComposeClient::RemoveActiveSession() {
  if (debug_session_) {
    debug_session_.reset();
    return;
  }
  if (!active_compose_ids_.has_value()) {
    return;
  }
  auto it = sessions_.find(active_compose_ids_.value().first);
  CHECK(it != sessions_.end())
      << "Attempted to remove compose session that doesn't exist.";
  sessions_.erase(active_compose_ids_.value().first);
  active_compose_ids_.reset();
}

void ChromeComposeClient::SetMSBBSessionCloseReason(
    compose::ComposeFreOrMsbbSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->SetMSBBCloseReason(close_reason);
  }
}

void ChromeComposeClient::SetFirstRunSessionCloseReason(
    compose::ComposeFreOrMsbbSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->SetFirstRunCloseReason(close_reason);
  }
}

void ChromeComposeClient::SetSessionCloseReason(
    compose::ComposeSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->SetCloseReason(close_reason);
  }
}

void ChromeComposeClient::LaunchHatsSurveyForActiveSession(
    compose::ComposeSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->LaunchHatsSurvey(close_reason);
  }
}

void ChromeComposeClient::RemoveAllSessions() {
  if (debug_session_) {
    debug_session_.reset();
  }

  sessions_.erase(sessions_.begin(), sessions_.end());
  active_compose_ids_.reset();
}

void ChromeComposeClient::ShowSavedStateNotification(
    autofill::FieldGlobalId field_id) {
  if (!active_compose_ids_.has_value()) {
    // Do not show the saved state notification on a previous field if another
    // autofill suggestion is showing in the newly focused field.
    return;
  }
  if (active_compose_ids_->first != field_id &&
      HasSession(active_compose_ids_->first)) {
    // Do not show the saved state notification on a previous field if focusing
    // on a new field that will show a compose nudge. Do not show nudge and
    // saved state notification on two different fields at the same time.
    return;
  }

  if (autofill::AutofillDriver* driver =
          autofill::ContentAutofillDriver::GetForRenderFrameHost(
              GetWebContents().GetPrimaryMainFrame())) {
    driver->RendererShouldTriggerSuggestions(
        field_id,
        autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus);
  }
}

ComposeSession* ChromeComposeClient::GetSessionForActiveComposeField() {
  if (active_compose_ids_.has_value()) {
    auto it = sessions_.find(active_compose_ids_.value().first);
    if (it != sessions_.end()) {
      return it->second.get();
    }
  }
  return nullptr;
}

bool ChromeComposeClient::IsPageLanguageSupported() {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  return compose_enabling_->IsPageLanguageSupported(translate_manager);
}

bool ChromeComposeClient::GetMSBBStateFromPrefs() {
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile_->GetPrefs());
  return !(helper != nullptr && !helper->IsEnabled());
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
}

ComposeEnabling& ChromeComposeClient::GetComposeEnabling() {
  return *compose_enabling_;
}

compose::PageUkmTracker* ChromeComposeClient::GetPageUkmTracker() {
  return page_ukm_tracker_.get();
}

bool ChromeComposeClient::ActiveFieldHasUnexpiredSession() {
  if (ComposeSession* current_session = GetSessionForActiveComposeField()) {
    return !current_session->HasExpired();
  }
  return false;
}

bool ChromeComposeClient::ShouldTriggerPopup(
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field_data,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  // Saved state notification needs the active field set earlier here at nudge
  // triggering, rather than later when the compose dialog is shown so that we
  // can know if the user focused on a different field.
  active_compose_ids_ = std::make_optional<FieldIdentifier>(
      form_field_data.global_id(), form_field_data.renderer_form_id());

  if (ActiveFieldHasUnexpiredSession()) {
    if (compose_enabling_->ShouldTriggerSavedStatePopup(trigger_source)) {
      last_popup_trigger_source_ = trigger_source;
      return true;
    }
    return false;
  }

  auto proactive_nudge_status = compose_enabling_->ShouldTriggerNoStatePopup(
      form_field_data.autocomplete_attribute(),
      form_field_data.allows_writing_suggestions(), profile_, pref_service_,
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents()),
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      form_field_data.origin(),
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL(),
      GetMSBBStateFromPrefs());

  compose::ProactiveNudgeTracker::Signals nudge_signals;
  nudge_signals.ukm_source_id =
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId();
  nudge_signals.page_origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  nudge_signals.page_url = web_contents()->GetURL();
  nudge_signals.form = form_data;
  nudge_signals.field = form_field_data;
  nudge_signals.page_change_time = page_change_time_;

  if (!proactive_nudge_status.has_value()) {
    compose::LogComposeProactiveNudgeShowStatus(proactive_nudge_status.error());
    // Record that the nudge could have shown if it was disabled by
    // configuration or flags.
    if (ComposeNudgeShowStatusDisabledByConfig(
            proactive_nudge_status.error())) {
      page_ukm_tracker_->ComposeProactiveNudgeShouldShow();
    }
    if (proactive_nudge_status.error() ==
            compose::ComposeShowStatus::kProactiveNudgeFeatureDisabled &&
        compose::GetComposeConfig().selection_nudge_enabled) {
      // If the proactive nudge is disabled but the selection nudge is is
      // enabled we need to initialize the nudge tracker for this form field to
      // accept the selection nudge.
      return nudge_tracker_.ProactiveNudgeRequestedForFormField(
          std::move(nudge_signals));
    }
    return false;
  }

  // ProactiveNudgeRequestedForFormField logs metrics for showing the nudge.
  if (nudge_tracker_.ProactiveNudgeRequestedForFormField(
          std::move(nudge_signals))) {
    last_popup_trigger_source_ = trigger_source;
    return true;
  }
  return false;
}

bool ChromeComposeClient::IsPopupTimerRunning() {
  return nudge_tracker_.IsTimerRunning();
}

void ChromeComposeClient::DisableProactiveNudge() {
  nudge_tracker_.OnUserDisabledNudge(/*single_site_only=*/false);
  proactive_nudge_enabled_.SetValue(false);

  switch (most_recent_nudge_entry_point_) {
    case compose::ComposeEntryPoint::kProactiveNudge:
      compose::LogComposeProactiveNudgeCtr(
          compose::ComposeNudgeCtrEvent::kUserDisabledProactiveNudge);
      GetPageUkmTracker()->ProactiveNudgeDisabledGlobally();
      break;
    case compose::ComposeEntryPoint::kSelectionNudge:
      compose::LogComposeSelectionNudgeCtr(
          compose::ComposeNudgeCtrEvent::kUserDisabledProactiveNudge);
      break;
    case compose::ComposeEntryPoint::kContextMenu:
    case compose::ComposeEntryPoint::kSavedStateNudge:
    case compose::ComposeEntryPoint::kSavedStateNotification:
      break;
  }

  if (base::FeatureList::IsEnabled(
          compose::features::kHappinessTrackingSurveysForComposeAcceptance)) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile_, /*create_if_necessary*/ true);
    if (hats_service) {
      hats_service->LaunchSurveyForWebContents(
          kHatsSurveyTriggerComposeNudgeClose, web_contents(), {}, {});
    }
  }
}

void ChromeComposeClient::OpenProactiveNudgeSettings() {
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  // `browser` should never be null here. This can only be triggered when there
  // is an active ComposeSession, which  is indirectly owned by the same
  // WebContents that holds the field that the Compose dialog is triggered from.
  // The session is created when that dialog is opened and it is destroyed if
  // its WebContents is destroyed.
  CHECK(browser);

  switch (most_recent_nudge_entry_point_) {
    case compose::ComposeEntryPoint::kProactiveNudge:
      compose::LogComposeProactiveNudgeCtr(
          compose::ComposeNudgeCtrEvent::kOpenSettings);
      break;
    case compose::ComposeEntryPoint::kSelectionNudge:
      compose::LogComposeSelectionNudgeCtr(
          compose::ComposeNudgeCtrEvent::kOpenSettings);
      break;
    case compose::ComposeEntryPoint::kContextMenu:
    case compose::ComposeEntryPoint::kSavedStateNudge:
    case compose::ComposeEntryPoint::kSavedStateNotification:
      break;
  }

  chrome::ShowSettingsSubPage(browser, chrome::kOfferWritingHelpSubpage);
}

void ChromeComposeClient::AddSiteToNeverPromptList(const url::Origin& origin) {
  nudge_tracker_.OnUserDisabledNudge(/*single_site_only=*/true);
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kProactiveNudgeDisabledSitesWithTime);
  update->Set(origin.Serialize(), base::TimeToValue(base::Time::Now()));

  switch (most_recent_nudge_entry_point_) {
    case compose::ComposeEntryPoint::kProactiveNudge:
      compose::LogComposeProactiveNudgeCtr(
          compose::ComposeNudgeCtrEvent::kUserDisabledSite);
      GetPageUkmTracker()->ProactiveNudgeDisabledForSite();
      break;
    case compose::ComposeEntryPoint::kSelectionNudge:
      compose::LogComposeSelectionNudgeCtr(
          compose::ComposeNudgeCtrEvent::kUserDisabledSite);
      break;
    case compose::ComposeEntryPoint::kContextMenu:
    case compose::ComposeEntryPoint::kSavedStateNudge:
    case compose::ComposeEntryPoint::kSavedStateNotification:
      break;
  }
}

bool ChromeComposeClient::ShouldTriggerContextMenu(
    content::RenderFrameHost* rfh,
    content::ContextMenuParams& params) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  bool allow_context_menu = compose_enabling_->ShouldTriggerContextMenu(
      profile_, translate_manager, rfh, params);
  if (allow_context_menu) {
    page_ukm_tracker_->MenuItemShown();
  }
  return allow_context_menu;
}

void ChromeComposeClient::OnSessionComplete(
    autofill::FieldGlobalId field_global_id,
    compose::ComposeSessionCloseReason close_reason,
    const compose::ComposeSessionEvents& events) {
  nudge_tracker_.ComposeSessionCompleted(field_global_id, close_reason, events);
}

void ChromeComposeClient::OnAfterFocusOnFormField(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field) {
  // Reset the `active_compose_ids_` on every focus change. This will be set to
  // a valid value when triggering a compose nudge or showing the compose
  // dialog.
  active_compose_ids_.reset();
}

optimization_guide::OptimizationGuideModelExecutor*
ChromeComposeClient::GetModelExecutor() {
  return model_executor_for_test_.value_or(
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())));
}

base::Token ChromeComposeClient::GetSessionId() {
  return session_id_for_test_.value_or(base::Token::CreateRandom());
}

optimization_guide::OptimizationGuideDecider*
ChromeComposeClient::GetOptimizationGuide() {
  return opt_guide_;
}

InnerTextProvider* ChromeComposeClient::GetInnerTextProvider() {
  return inner_text_provider_for_test_.value_or(this);
}

void ChromeComposeClient::SetModelExecutorForTest(
    optimization_guide::OptimizationGuideModelExecutor* model_executor) {
  model_executor_for_test_ = model_executor;
}

void ChromeComposeClient::SetSkipShowDialogForTest(bool should_skip) {
  skip_show_dialog_for_test_ = should_skip;
}

void ChromeComposeClient::SetSessionIdForTest(base::Token session_id) {
  session_id_for_test_ = session_id;
}
void ChromeComposeClient::SetInnerTextProviderForTest(
    InnerTextProvider* inner_text) {
  inner_text_provider_for_test_ = inner_text;
}

bool ChromeComposeClient::IsDialogShowing() {
  return compose_dialog_controller_ &&
         compose_dialog_controller_->IsDialogShowing();
}

int ChromeComposeClient::GetSessionCountForTest() {
  return sessions_.size();
}

void ChromeComposeClient::OpenFeedbackPageForTest(std::string feedback_id) {
  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->OpenFeedbackPage(feedback_id);
  }
}

void ChromeComposeClient::PrimaryPageChanged(content::Page& page) {
  RemoveAllSessions();

  page_ukm_tracker_ = std::make_unique<compose::PageUkmTracker>(
      page.GetMainDocument().GetPageUkmSourceId());

  nudge_tracker_.Clear();

  compose::ComposeTextUsageLogger::GetOrCreateForCurrentDocument(
      &page.GetMainDocument());

  page_change_time_ = base::TimeTicks::Now();
}

void ChromeComposeClient::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (!compose_enabling_->IsEnabledForProfile(profile_)) {
    return;
  }
  ComposeSession* active_session = GetSessionForActiveComposeField();
  if (open_settings_requested_) {
    open_settings_requested_ = false;

    if (active_session && !active_session->get_current_msbb_state() &&
        active_compose_ids_.has_value()) {
      content::RenderFrameHost* top_level_frame =
          GetWebContents().GetPrimaryMainFrame();
      if (auto* driver = autofill::ContentAutofillDriver::GetForRenderFrameHost(
              top_level_frame)) {
        GetManager().OpenCompose(
            *driver, active_compose_ids_.value().second,
            active_compose_ids_.value().first,
            compose::ComposeManagerImpl::UiEntryPoint::kContextMenu);
      }
    }
  }
}

void ChromeComposeClient::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (IsDialogShowing() &&
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    // TODO(b/318571287): Log when the dialog is closed due to scrolling.
    compose_dialog_controller_->Close();
  }
}
void ChromeComposeClient::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  // TODO(crbug/337690061): Use Autofill events to track focus change.
  return nudge_tracker_.FocusChangedInPage();
}

void ChromeComposeClient::ShowProactiveNudge(
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    compose::ComposeEntryPoint entry_point) {
  if (autofill::AutofillDriver* driver =
          autofill::ContentAutofillDriver::GetForRenderFrameHost(
              GetWebContents().GetPrimaryMainFrame())) {
    driver->RendererShouldTriggerSuggestions(
        field, autofill::AutofillSuggestionTriggerSource::
                   kComposeDelayedProactiveNudge);
  }
  most_recent_nudge_entry_point_ = entry_point;
}

compose::ComposeHintMetadata ChromeComposeClient::GetComposeHintMetadata() {
  if (!opt_guide_) {
    return compose::ComposeHintMetadata::default_instance();
  }

  optimization_guide::OptimizationMetadata opt_guide_metadata;
  auto opt_guide_has_hint = opt_guide_->CanApplyOptimization(
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL(),
      optimization_guide::proto::OptimizationType::COMPOSE,
      &opt_guide_metadata);
  if (opt_guide_has_hint !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return compose::ComposeHintMetadata::default_instance();
  }

  if (opt_guide_metadata.any_metadata().has_value()) {
    std::optional<compose::ComposeHintMetadata> compose_metadata =
        optimization_guide::ParsedAnyMetadata<compose::ComposeHintMetadata>(
            opt_guide_metadata.any_metadata().value());
    if (compose_metadata.has_value()) {
      return compose_metadata.value();
    }
  }

  return compose::ComposeHintMetadata::default_instance();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
