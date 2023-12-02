// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_text_usage_logger.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/compose/type_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
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

const char kComposeURL[] = "chrome://compose/";

bool ShouldResumeSessionFromEntryPoint(
    ChromeComposeClient::EntryPoint entry_point) {
  switch (entry_point) {
    case ChromeComposeClient::EntryPoint::kAutofillPopup:
      return true;
    case ChromeComposeClient::EntryPoint::kContextMenu:
      return false;
  }
}

}  // namespace

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      translate_language_provider_(new TranslateLanguageProvider()),
      manager_(this),
      client_page_receiver_(this) {
  profile_ = Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  pref_service_ = profile_->GetPrefs();
  compose_enabling_ = std::make_unique<ComposeEnabling>(
      translate_language_provider_.get(), profile_);

  if (GetOptimizationGuide()) {
    std::vector<optimization_guide::proto::OptimizationType> types;
    if (compose_enabling_->IsEnabledForProfile(profile_).has_value()) {
      types.push_back(optimization_guide::proto::OptimizationType::COMPOSE);
    }

    if (!types.empty()) {
      GetOptimizationGuide()->RegisterOptimizationTypes(types);
    }
  }
}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeClientPageHandler>
        client_handler,
    mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  client_page_receiver_.reset();
  client_page_receiver_.Bind(std::move(client_handler));

  url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (origin == url::Origin::Create(GURL(kComposeURL))) {
    debug_session_ = std::make_unique<ComposeSession>(
        &GetWebContents(), GetModelExecutor(), GetModelQualityLogsUploader(),
        GetSessionId());
    debug_session_->set_skip_inner_text(true);
    debug_session_->Bind(std::move(handler), std::move(dialog));
    return;
  }
  sessions_.at(active_compose_field_id_.value())
      ->Bind(std::move(handler), std::move(dialog));
}

void ChromeComposeClient::ShowComposeDialog(
    EntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeCallback callback) {
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
    // TODO(b/312295685): Add metrics for consent dialog related close reasons.
    case compose::mojom::CloseReason::kConsentCloseButton:
    case compose::mojom::CloseReason::kPageContentConsentDeclined:
      RemoveActiveSession();
      break;
    case compose::mojom::CloseReason::kCloseButton:
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kCloseButtonPressed);
      RemoveActiveSession();
      break;
    case compose::mojom::CloseReason::kInsertButton:
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kAcceptedSuggestion);
      RemoveActiveSession();
      break;
  }

  if (compose_dialog_controller_) {
    compose_dialog_controller_->Close();
  }
}

void ChromeComposeClient::ApproveConsent() {
  pref_service_->SetBoolean(
      unified_consent::prefs::kPageContentCollectionEnabled, true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  pref_service_->SetBoolean(prefs::kPrefHasAcceptedComposeConsent, true);
#endif
  UpdateAllSessionsWithConsentApproved();
}

void ChromeComposeClient::AcknowledgeConsentDisclaimer() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  pref_service_->SetBoolean(prefs::kPrefHasAcceptedComposeConsent, true);
#endif
  UpdateAllSessionsWithConsentApproved();
}

void ChromeComposeClient::UpdateAllSessionsWithConsentApproved() {
  for (const auto& session : sessions_) {
    session.second->set_consent_given_or_acknowledged();
  }
}

void ChromeComposeClient::CreateOrUpdateSession(
    EntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    ComposeCallback callback) {
  active_compose_field_id_ =
      std::make_optional<autofill::FieldGlobalId>(trigger_field.global_id());
  std::string selected_text = base::UTF16ToUTF8(trigger_field.selected_text);
  ComposeSession* current_session;

  // We only want to resume if the popup was clicked or the selection is empty.
  // If the context menu were clicked with a selection, presume this is intent
  // to restart using the new selection.
  bool resume_current_session =
      ShouldResumeSessionFromEntryPoint(ui_entry_point) ||
      selected_text.empty();

  bool has_session = HasSession(active_compose_field_id_.value());
  if (has_session && resume_current_session) {
    auto it = sessions_.find(active_compose_field_id_.value());
    current_session = it->second.get();
    current_session->set_compose_callback(std::move(callback));
  } else {
    if (has_session) {
      // We have a session already, and we are going to close it and create a
      // new one, which will require a close reason.
      SetSessionCloseReason(
          compose::ComposeSessionCloseReason::kNewSessionWithSelectedText);
    }
    auto new_session = std::make_unique<ComposeSession>(
        &GetWebContents(), GetModelExecutor(), GetModelQualityLogsUploader(),
        GetSessionId(), std::move(callback));
    current_session = new_session.get();
    // Insert or replace with a new session.
    sessions_.insert_or_assign(active_compose_field_id_.value(),
                               std::move(new_session));

    // Only record the selection length for new sessions.
    auto utf8_chars = base::CountUnicodeCharacters(selected_text);
    compose::LogComposeDialogSelectionLength(
        utf8_chars.has_value() ? utf8_chars.value() : 0);
  }

  current_session->set_initial_consent_state(GetConsentStateFromPrefs());

  // If we are resuming then don't send the selected text - we want to keep the
  // prior selection and not trigger another Compose.
  current_session->InitializeWithText(resume_current_session
                                          ? std::nullopt
                                          : std::make_optional(selected_text));
}

void ChromeComposeClient::RemoveActiveSession() {
  if (debug_session_) {
    debug_session_.reset();
    return;
  }
  auto it = sessions_.find(active_compose_field_id_.value());
  CHECK(it != sessions_.end())
      << "Attempted to remove compose session that doesn't exist.";
  sessions_.erase(active_compose_field_id_.value());
  active_compose_field_id_.reset();
}

void ChromeComposeClient::SetSessionCloseReason(
    compose::ComposeSessionCloseReason close_reason) {
  if (debug_session_) {
    return;
  }

  if (active_compose_field_id_.has_value()) {
    auto it = sessions_.find(active_compose_field_id_.value());
    if (it != sessions_.end()) {
      it->second->SetCloseReason(close_reason);
    }
  }
}

void ChromeComposeClient::RemoveAllSessions() {
  if (debug_session_) {
    debug_session_.reset();
  }

  sessions_.erase(sessions_.begin(), sessions_.end());
  active_compose_field_id_.reset();
}

compose::mojom::ConsentState ChromeComposeClient::GetConsentStateFromPrefs() {
  auto consent_state = compose::mojom::ConsentState::kUnset;
  bool page_content_collection_enabled = pref_service_->GetBoolean(
      unified_consent::prefs::kPageContentCollectionEnabled);
  bool consent_acknowledged_through_compose = false;
  consent_acknowledged_through_compose =
      pref_service_->GetBoolean(prefs::kPrefHasAcceptedComposeConsent);
  if (page_content_collection_enabled) {
    // Page content collection can be enabled from the Compose UI or through
    // other UIs. If the latter, then a specific disclaimer dialog should be
    // shown for Compose FRE. This is captured by `consent_state`.
    consent_state = consent_acknowledged_through_compose
                        ? compose::mojom::ConsentState::kConsented
                        : compose::mojom::ConsentState::kExternalConsented;
  }
  return consent_state;
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
}

ComposeEnabling& ChromeComposeClient::GetComposeEnabling() {
  return *compose_enabling_;
}

bool ChromeComposeClient::ShouldTriggerPopup(
    const autofill::FormFieldData& form_field_data) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  content::RenderFrameHost* top_level_frame =
      GetWebContents().GetPrimaryMainFrame();

  GURL url = GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL();

  return compose_enabling_->ShouldTriggerPopup(
      form_field_data.autocomplete_attribute, profile_, translate_manager,
      HasSession(form_field_data.global_id()),
      top_level_frame->GetLastCommittedOrigin(), form_field_data.origin, url);
}

bool ChromeComposeClient::ShouldTriggerContextMenu(
    content::RenderFrameHost* rfh,
    content::ContextMenuParams& params) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  return compose_enabling_->ShouldTriggerContextMenu(
      profile_, translate_manager, rfh, params);
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

int ChromeComposeClient::GetSessionCountForTest() {
  return sessions_.size();
}

void ChromeComposeClient::OpenFeedbackPageForTest(std::string feedback_id) {
  if (active_compose_field_id_.has_value()) {
    auto it = sessions_.find(active_compose_field_id_.value());
    if (it != sessions_.end()) {
      it->second->OpenFeedbackPage(feedback_id);
    }
  }
}

void ChromeComposeClient::PrimaryPageChanged(content::Page& page) {
  RemoveAllSessions();

  compose::ComposeTextUsageLogger::GetOrCreateForCurrentDocument(
      &page.GetMainDocument());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
