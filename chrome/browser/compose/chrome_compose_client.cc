// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/third_party/icu/icu_utf.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_text_usage_logger.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/common/compose/type_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
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

bool ShouldResumeSessionFromEntryPoint(
    ChromeComposeClient::EntryPoint entry_point) {
  switch (entry_point) {
    case ChromeComposeClient::EntryPoint::kAutofillPopup:
      return true;
    case ChromeComposeClient::EntryPoint::kContextMenu:
      return false;
  }
}

std::u16string RemoveLastCharIfInvalid(std::u16string str) {
  // TODO(b/323902463): Have Autofill send a valid string, i.e. truncated to a
  // valid grapheme, in FormFieldData.selected_text to ensure greatest
  // preservation of the original selected text.
  if (!str.empty() && CBU16_IS_LEAD(str.back())) {
    str.pop_back();
  }
  return str;
}

}  // namespace

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      translate_language_provider_(new TranslateLanguageProvider()),
      manager_(this),
      client_page_receiver_(this) {
  auto ukm_source_id =
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId();
  page_ukm_tracker_ = std::make_unique<compose::PageUkmTracker>(ukm_source_id);
  profile_ = Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  pref_service_ = profile_->GetPrefs();
  compose_enabling_ = std::make_unique<ComposeEnabling>(
      translate_language_provider_.get(), profile_,
      IdentityManagerFactory::GetForProfileIfExists(profile_),
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
}

ChromeComposeClient::~ChromeComposeClient() = default;

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
        &GetWebContents(), GetModelExecutor(), GetModelQualityLogsUploader(),
        GetSessionId(), GetInnerTextProvider(), autofill::FieldRendererId(-1));
    debug_session_->set_collect_inner_text(false);
    debug_session_->set_fre_complete(
        pref_service_->GetBoolean(prefs::kPrefHasCompletedComposeFRE));
    debug_session_->set_current_msbb_state(GetMSBBStateFromPrefs());
    debug_session_->Bind(std::move(handler), std::move(dialog));
    return;
  }
  sessions_.at(active_compose_ids_.value().first)
      ->Bind(std::move(handler), std::move(dialog));
}

void ChromeComposeClient::ShowComposeDialog(
    EntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeCallback callback) {
  // Do not show multiple dialogs at the same time.
  if (IsDialogShowing() &&
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification)) {
    compose_dialog_controller_->Close();
  }

  CreateOrUpdateSession(ui_entry_point, trigger_field, std::move(callback));
  if (!skip_show_dialog_for_test_) {
    // The bounds given by autofill are relative to the top level frame. Here we
    // offset by the WebContents container to make up for that.
    gfx::RectF bounds_in_screen = trigger_field.bounds;
    bounds_in_screen.Offset(
        GetWebContents().GetContainerBounds().OffsetFromOrigin());

    show_dialog_start_ = base::TimeTicks::Now();
    compose_dialog_controller_ =
        chrome::ShowComposeDialog(GetWebContents(), bounds_in_screen);
  }
}

bool ChromeComposeClient::HasSession(
    const autofill::FieldGlobalId& trigger_field_id) {
  auto it = sessions_.find(trigger_field_id);
  return it != sessions_.end();
}

void ChromeComposeClient::ShowUI() {
  if (compose_dialog_controller_) {
    compose_dialog_controller_->ShowUI();
    compose::LogComposeDialogOpenLatency(base::TimeTicks::Now() -
                                         show_dialog_start_);
  }
}

void ChromeComposeClient::CloseUI(compose::mojom::CloseReason reason) {
  switch (reason) {
    case compose::mojom::CloseReason::kFirstRunCloseButton:
      SetFirstRunSessionCloseReason(
          compose::ComposeFirstRunSessionCloseReason::kCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kMSBBCloseButton:
      SetMSBBSessionCloseReason(
          compose::ComposeMSBBSessionCloseReason::kMSBBCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kCloseButton:
      base::RecordAction(
          base::UserMetricsAction("Compose.EndedSession.CloseButtonClicked"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kCloseButtonPressed);
      break;
    case compose::mojom::CloseReason::kInsertButton:
      base::RecordAction(
          base::UserMetricsAction("Compose.EndedSession.InsertButtonClicked"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kAcceptedSuggestion);
      SetMSBBSessionCloseReason(
          compose::ComposeMSBBSessionCloseReason::kMSBBAcceptedWithInsert);
      SetFirstRunSessionCloseReason(
          compose::ComposeFirstRunSessionCloseReason::
              kFirstRunDisclaimerAcknowledgedWithInsert);
      page_ukm_tracker_->ComposeTextInserted();
      break;
    case compose::mojom::CloseReason::kLostFocus:
      break;
  }

  if (reason != compose::mojom::CloseReason::kLostFocus) {
    // Do not remove session when closing after showing the saved state
    // notification.
    RemoveActiveSession();
  }

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
  ComposeSession* active_session = GetSessionForActiveComposeField();
  open_settings_requested_ = false;

  if (active_session) {
    active_session->SetFirstRunCloseReason(
        compose::ComposeFirstRunSessionCloseReason::
            kFirstRunDisclaimerAcknowledgedWithoutInsert);
  }
}

void ChromeComposeClient::OpenComposeSettings() {
  auto* browser = chrome::FindBrowserWithTab(&GetWebContents());
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
    absl::optional<int> node_id,
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

void ChromeComposeClient::CreateOrUpdateSession(
    EntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    ComposeCallback callback) {
  active_compose_ids_ = std::make_optional<
      std::pair<autofill::FieldGlobalId, autofill::FormGlobalId>>(
      trigger_field.global_id(), trigger_field.renderer_form_id());
  // The selected text received from Autofill is a UTF-16 string truncated using
  // substr, which will result in a rendered invalid character in the Compose
  // dialog if it splits a surrogate pair character. Ensure that any invalid
  // characters are removed.
  std::string selected_text =
      base::UTF16ToUTF8(RemoveLastCharIfInvalid(trigger_field.selected_text));

  ComposeSession* current_session;

  // We only want to resume if the popup was clicked or the selection is empty.
  // If the context menu were clicked with a selection, presume this is intent
  // to restart using the new selection.
  bool resume_current_session =
      ShouldResumeSessionFromEntryPoint(ui_entry_point) ||
      selected_text.empty();

  bool has_session = HasSession(active_compose_ids_.value().first);
  if (has_session && resume_current_session) {
    auto it = sessions_.find(active_compose_ids_.value().first);
    current_session = it->second.get();
    current_session->set_compose_callback(std::move(callback));
  } else {
    if (has_session) {
      // We have a session already, and we are going to close it and create a
      // new one, which will require a close reason.
      base::RecordAction(base::UserMetricsAction(
          "Compose.EndedSession.NewSessionWithSelectedText"));
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kNewSessionWithSelectedText);
      // Set the equivalent close reason if the existing session was in a
      // consent state.
      auto it = sessions_.find(active_compose_ids_.value().first);
      current_session = it->second.get();
      if (!current_session->get_fre_complete()) {
        SetFirstRunSessionCloseReason(
            compose::ComposeFirstRunSessionCloseReason::
                kNewSessionWithSelectedText);
      }
    }
    // Now create and set up a new session.
    auto new_session = std::make_unique<ComposeSession>(
        &GetWebContents(), GetModelExecutor(), GetModelQualityLogsUploader(),
        GetSessionId(), GetInnerTextProvider(),
        trigger_field.global_id().renderer_id, std::move(callback));
    current_session = new_session.get();
    sessions_.insert_or_assign(active_compose_ids_.value().first,
                               std::move(new_session));

    // Set the FRE state of the new session.
    auto fre_state =
        pref_service_->GetBoolean(prefs::kPrefHasCompletedComposeFRE);
    current_session->set_fre_complete(fre_state);

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
  }  // End of create new session.
  current_session->set_current_msbb_state(GetMSBBStateFromPrefs());

  // If we are resuming then don't send the selected text - we want to keep the
  // prior selection and not trigger another Compose.
  current_session->InitializeWithText(
      resume_current_session ? std::nullopt : std::make_optional(selected_text),
      !selected_text.empty());
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
    compose::ComposeMSBBSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  ComposeSession* active_session = GetSessionForActiveComposeField();

  if (active_session) {
    active_session->SetMSBBCloseReason(close_reason);
  }
}

void ChromeComposeClient::SetFirstRunSessionCloseReason(
    compose::ComposeFirstRunSessionCloseReason close_reason) {
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

void ChromeComposeClient::RemoveAllSessions() {
  if (debug_session_) {
    debug_session_.reset();
  }

  sessions_.erase(sessions_.begin(), sessions_.end());
  active_compose_ids_.reset();
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

compose::PageUkmTracker* ChromeComposeClient::getPageUkmTracker() {
  return page_ukm_tracker_.get();
}

bool ChromeComposeClient::ShouldTriggerPopup(
    const autofill::FormFieldData& form_field_data) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  content::RenderFrameHost* top_level_frame =
      GetWebContents().GetPrimaryMainFrame();

  GURL url = GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL();

  bool should_trigger_popup = compose_enabling_->ShouldTriggerPopup(
      form_field_data.autocomplete_attribute, profile_, translate_manager,
      HasSession(form_field_data.global_id()),
      top_level_frame->GetLastCommittedOrigin(), form_field_data.origin, url);

  if (IsDialogShowing() && should_trigger_popup &&
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification)) {
    // If there is a current dialog showing and we are about to show the nudge,
    // close the current dialog so that both are not shown at the same time.
    compose_dialog_controller_->Close();
  }

  return should_trigger_popup;
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

optimization_guide::ModelQualityLogsUploader*
ChromeComposeClient::GetModelQualityLogsUploader() {
  return model_quality_uploader_for_test_.value_or(
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())));
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

void ChromeComposeClient::SetModelQualityLogsUploaderForTest(
    optimization_guide::ModelQualityLogsUploader* model_quality_uploader) {
  model_quality_uploader_for_test_ = model_quality_uploader;
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

  if (IsDialogShowing() &&
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification)) {
    // Close the dialog on navigation.
    compose_dialog_controller_->Close();
  }

  compose::ComposeTextUsageLogger::GetOrCreateForCurrentDocument(
      &page.GetMainDocument());
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

void ChromeComposeClient::OnVisibilityChanged(content::Visibility visibility) {
  if (IsDialogShowing() && visibility != content::Visibility::VISIBLE &&
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification)) {
    // Close the dialog when the WebContents is no longer visible.
    compose_dialog_controller_->Close();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
