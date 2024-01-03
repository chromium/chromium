// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/compose/type_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

bool IsValidComposePrompt(const std::string& prompt) {
  const compose::Config& config = compose::GetComposeConfig();
  if (prompt.length() > config.input_max_chars) {
    return false;
  }

  base::StringTokenizer tokenizer(
      prompt, " ", base::StringTokenizer::WhitespacePolicy::kSkipOver);
  unsigned int word_count = 0;
  while (tokenizer.GetNext()) {
    ++word_count;
    if (word_count > config.input_max_words) {
      return false;
    }
  }

  if (word_count < config.input_min_words) {
    return false;
  }
  return true;
}

const char kComposeBugReportURL[] = "https://goto.google.com/ccbrfd";
const char kComposeFeedbackSurveyURL[] = "https://goto.google.com/ccfsfd";

void LogComposeResponseStatus(compose::mojom::ComposeStatus status) {
  UMA_HISTOGRAM_ENUMERATION(compose::kComposeResponseStatus, status);
}

}  // namespace

// The state of a compose session. This currently includes the model quality log
// entry, and the mojo based compose state.
class ComposeState {
 public:
  ComposeState() {
    modeling_log_entry_ = nullptr;
    mojo_state_ = nullptr;
  }

  ComposeState(std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                   modeling_log_entry,
               compose::mojom::ComposeStatePtr mojo_state) {
    modeling_log_entry_ = std::move(modeling_log_entry);
    mojo_state_ = std::move(mojo_state);
  }

  ~ComposeState() = default;

  bool IsMojoValid() {
    return (mojo_state_ && mojo_state_->response &&
            mojo_state_->response->status ==
                compose::mojom::ComposeStatus::kOk &&
            mojo_state_->response->result != "");
  }

  optimization_guide::ModelQualityLogEntry* modeling_log_entry() {
    return modeling_log_entry_.get();
  }
  std::unique_ptr<optimization_guide::ModelQualityLogEntry>
  TakeModelingLogEntry() {
    auto to_return = std::move(modeling_log_entry_);
    return to_return;
  }

  void SetModelingLogEntry(
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>
          modeling_log_entry) {
    modeling_log_entry_ = std::move(modeling_log_entry);
  }

  compose::mojom::ComposeState* mojo_state() { return mojo_state_.get(); }
  compose::mojom::ComposeStatePtr TakeMojoState() {
    auto to_return = std::move(mojo_state_);
    return to_return;
  }

  void SetMojoState(compose::mojom::ComposeStatePtr mojo_state) {
    mojo_state_ = std::move(mojo_state);
  }

 private:
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> modeling_log_entry_;
  compose::mojom::ComposeStatePtr mojo_state_;
};

ComposeSession::ComposeSession(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideModelExecutor* executor,
    optimization_guide::ModelQualityLogsUploader* model_quality_logs_uploader,
    base::Token session_id,
    ComposeCallback callback)
    : executor_(executor),
      handler_receiver_(this),
      close_reason_(compose::ComposeSessionCloseReason::kEndedImplicitly),
      final_status_(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED),
      web_contents_(web_contents),
      model_quality_logs_uploader_(model_quality_logs_uploader),
      session_id_(session_id),
      weak_ptr_factory_(this) {
  callback_ = std::move(callback);
  current_state_ = compose::mojom::ComposeState::New();
  most_recent_ok_state_ = std::make_unique<ComposeState>();
  if (executor_) {
    session_ = executor_->StartSession(
        optimization_guide::proto::ModelExecutionFeature::
            MODEL_EXECUTION_FEATURE_COMPOSE);
  }
}

ComposeSession::~ComposeSession() {
  // Don't log any metrics for sessions that only display consent/disclaimer
  // dialogs.
  // TODO(b/312295685): Add metrics for consent dialog related close reasons.
  if (initial_consent_state_ != compose::mojom::ConsentState::kConsented &&
      !consent_given_or_acknowledged_) {
    return;
  }

  LogComposeSessionCloseMetrics(close_reason_, compose_count_,
                                dialog_shown_count_, undo_count_);
  // If we have a modeling quality log entry, upload it.
  if (most_recent_ok_state_->modeling_log_entry()) {
    most_recent_ok_state_->modeling_log_entry()
        ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_final_status(final_status_);
    // Quality log would automaticlaly be uploaded on the destruction of
    // a modeling_log_entry. However in order to more easily test the qulity
    // uploads we are calling upload directly here.
    if (model_quality_logs_uploader_) {
      model_quality_logs_uploader_->UploadModelQualityLogs(
          most_recent_ok_state_->TakeModelingLogEntry());
    }
  }

  // Explicitly upload the rest of the undo stack.
  while (!undo_states_.empty()) {
    if (undo_states_.top()->modeling_log_entry()) {
      if (model_quality_logs_uploader_) {
        model_quality_logs_uploader_->UploadModelQualityLogs(
            undo_states_.top()->TakeModelingLogEntry());
      }
    }
    undo_states_.pop();
  }
}

void ComposeSession::Bind(
    mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  handler_receiver_.reset();
  handler_receiver_.Bind(std::move(handler));

  dialog_remote_.reset();
  dialog_remote_.Bind(std::move(dialog));
}

// ComposeSessionPageHandler
void ComposeSession::Compose(const std::string& input, bool is_input_edited) {
  optimization_guide::proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input(input);
  MakeRequest(std::move(request), is_input_edited);
}

void ComposeSession::Rewrite(compose::mojom::StyleModifiersPtr style) {
  optimization_guide::proto::ComposeRequest request;
  if (style->is_tone()) {
    request.mutable_rewrite_params()->set_tone(
        optimization_guide::proto::ComposeTone(style->get_tone()));
  } else if (style->is_length()) {
    request.mutable_rewrite_params()->set_length(
        optimization_guide::proto::ComposeLength(style->get_length()));
  }
  request.mutable_rewrite_params()->set_previous_response(
      most_recent_ok_state_->mojo_state()->response->result);
  MakeRequest(std::move(request), false);
}

void ComposeSession::MakeRequest(
    optimization_guide::proto::ComposeRequest request,
    bool is_input_edited) {
  current_state_->has_pending_request = true;
  current_state_->feedback =
      compose::mojom::UserFeedback::kUserFeedbackUnspecified;
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!session_ ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    ProcessError(compose::mojom::ComposeStatus::kMisconfiguration);
    return;
  }

  // Increase compose count regradless of status of request.
  compose_count_ += 1;

  if (skip_inner_text_ || inner_text_.has_value()) {
    RequestWithSession(std::move(request), is_input_edited);
  } else {
    // Prepare the compose call, which will be invoked when inner text
    // extraction is completed.
    continue_compose_ = base::BindOnce(&ComposeSession::RequestWithSession,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(request), is_input_edited);
  }
}

void ComposeSession::RequestWithSession(
    const optimization_guide::proto::ComposeRequest& request,
    bool is_input_edited) {
  if (skip_inner_text_) {
    // Make sure context is added for sessions with no inner text.
    AddPageContentToSession("");
  }

  base::ElapsedTimer request_timer;
  request_id_++;

  session_->ExecuteModel(
      request, base::BindRepeating(&ComposeSession::ModelExecutionCallback,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(request_timer), request_id_,
                                   is_input_edited));
}

void ComposeSession::ModelExecutionCallback(
    const base::ElapsedTimer& request_timer,
    int request_id,
    bool was_input_edited,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::TimeDelta request_delta = request_timer.Elapsed();

  // A new request has been issued, ignore this one.
  if (request_id != request_id_) {
    SendQualityLogEntryUponError(std::move(log_entry), request_delta);
    return;
  }

  current_state_->has_pending_request = false;

  compose::mojom::ComposeStatus status =
      ComposeStatusFromOptimizationGuideResult(result);

  if (status != compose::mojom::ComposeStatus::kOk) {
    compose::LogComposeRequestDuration(request_delta, /* is_valid */ false);
    ProcessError(status);
    SendQualityLogEntryUponError(std::move(log_entry), request_delta);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result->response);

  if (!response) {
    compose::LogComposeRequestDuration(request_delta, /* is_valid */ false);
    ProcessError(compose::mojom::ComposeStatus::kTryAgain);
    SendQualityLogEntryUponError(std::move(log_entry), request_delta);
    return;
  }
  DCHECK(result->is_complete ||
         base::FeatureList::IsEnabled(
             optimization_guide::features::kOptimizationGuideOnDeviceModel));

  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();
  current_state_->response = ui_response->Clone();
  if (result->is_complete) {
    // Log successful response status.
    LogComposeResponseStatus(compose::mojom::ComposeStatus::kOk);
    compose::LogComposeRequestDuration(request_delta, /* is_valid */ true);

    SaveMostRecentOkStateToUndoStack();
    most_recent_ok_state_->SetMojoState(current_state_->Clone());
  }
  ui_response->undo_available = !undo_states_.empty();
  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(std::move(ui_response));
  }

  if (log_entry) {
    log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_was_generated_via_edit(was_input_edited);
    log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_request_latency_ms(request_delta.InMilliseconds());
    optimization_guide::proto::Int128* token =
        log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
            ->mutable_session_id();

    token->set_high(session_id_.high());
    token->set_low(session_id_.low());
    most_recent_ok_state_->SetModelingLogEntry(std::move(log_entry));
  }
}

void ComposeSession::ProcessError(compose::mojom::ComposeStatus error) {
  LogComposeResponseStatus(error);

  current_state_->has_pending_request = false;
  current_state_->response = compose::mojom::ComposeResponse::New();
  current_state_->response->status = error;

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(current_state_->response->Clone());
  }
}

void ComposeSession::RequestInitialState(RequestInitialStateCallback callback) {
  if (current_state_->response) {
    current_state_->response->undo_available = !undo_states_.empty();
  }
  auto compose_config = compose::GetComposeConfig();
  std::move(callback).Run(compose::mojom::OpenMetadata::New(
      initial_consent_state_, initial_input_, current_state_->Clone(),
      compose::mojom::ConfigurableParams::New(compose_config.input_min_words,
                                              compose_config.input_max_words,
                                              compose_config.input_max_chars)));
}

void ComposeSession::SaveWebUIState(const std::string& webui_state) {
  current_state_->webui_state = webui_state;
}

void ComposeSession::AcceptComposeResult(
    AcceptComposeResultCallback success_callback) {
  if (callback_.is_null() || !current_state_->response ||
      current_state_->response->status != compose::mojom::ComposeStatus::kOk) {
    // Guard against invoking twice before the UI is able to disconnect.
    std::move(success_callback).Run(false);
    return;
  }
  std::move(callback_).Run(base::UTF8ToUTF16(current_state_->response->result));
  std::move(success_callback).Run(true);
}

void ComposeSession::Undo(UndoCallback callback) {
  if (undo_states_.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Only increase undo count if there are states to undo.
  undo_count_ += 1;

  std::unique_ptr<ComposeState> undo_state = std::move(undo_states_.top());
  undo_states_.pop();

  // upload the most recent modeling quality log entry before overwriting it
  // with state from undo,
  if (most_recent_ok_state_->modeling_log_entry() &&
      model_quality_logs_uploader_) {
    model_quality_logs_uploader_->UploadModelQualityLogs(
        most_recent_ok_state_->TakeModelingLogEntry());
  }

  if (!undo_state->IsMojoValid()) {
    // Gracefully fail if we find an invalid state on the undo stack.
    std::move(callback).Run(nullptr);
    return;
  }

  // State returns to the last undo_state.
  current_state_ = undo_state->mojo_state()->Clone();

  undo_state->mojo_state()->response->undo_available = !undo_states_.empty();

  std::move(callback).Run(undo_state->mojo_state()->Clone());
  // set recent state to the last undo modeling entry and last mojo state.
  most_recent_ok_state_->SetMojoState(undo_state->TakeMojoState());
  most_recent_ok_state_->SetModelingLogEntry(
      undo_state->TakeModelingLogEntry());
}

void ComposeSession::OpenBugReportingLink() {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(kComposeBugReportURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenFeedbackSurveyLink() {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(kComposeFeedbackSurveyURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenFeedbackPage(std::string feedback_id) {
  Browser* browser = chrome::FindLastActive();
  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", feedback_id);
  chrome::ShowFeedbackPage(
      browser, chrome::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_COMPOSE_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"compose",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void ComposeSession::SetUserFeedback(compose::mojom::UserFeedback feedback) {
  if (!most_recent_ok_state_->mojo_state()) {
    // If there is no recent State there is nothing that we should be applying
    // feedback to.
    return;
  }
  OptimizationGuideKeyedService* opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!opt_guide_keyed_service ||
      !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForLogging(
          optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE)) {
    return;
  }

  // Add to most_recent_ok_state_ in case of undos.
  most_recent_ok_state_->mojo_state()->feedback = feedback;

  // Add to current_state_ in case of coming back to a saved state, as
  // RequestInitialState() returns current_state_.
  if (current_state_->response) {
    current_state_->feedback = feedback;
  }
  optimization_guide::proto::UserFeedback user_feedback =
      OptimizationFeedbackFromComposeFeedback(feedback);

  if (most_recent_ok_state_->modeling_log_entry()) {
    optimization_guide::proto::ComposeQuality* quality =
        most_recent_ok_state_->modeling_log_entry()
            ->quality_data<optimization_guide::ComposeFeatureTypeMap>();
    if (quality) {
      quality->set_user_feedback(user_feedback);
    }
    if (feedback == compose::mojom::UserFeedback::kUserFeedbackNegative) {
      // Open the Feedback Page for a thumbs down using current request log.
      std::string feedback_id = most_recent_ok_state_->modeling_log_entry()
                                    ->log_ai_data_request()
                                    ->mutable_model_execution_info()
                                    ->server_execution_id();
      OpenFeedbackPage(feedback_id);
    }
  }
}

void ComposeSession::InitializeWithText(
    const std::optional<std::string>& text) {
  dialog_shown_count_ += 1;
  RefreshInnerText();

  // If no text provided (even an empty string), then we are reopening without
  // calling compose again, or updating the input text..
  if (!text.has_value()) {
    return;
  }

  initial_input_ = text.value();

  if (!IsValidComposePrompt(initial_input_) ||
      !compose::GetComposeConfig().auto_submit_with_selection ||
      initial_consent_state_ != compose::mojom::ConsentState::kConsented) {
    return;
  }

  Compose(initial_input_, false);
}

void ComposeSession::OpenComposeSettings() {
  auto* browser = chrome::FindBrowserWithTab(web_contents_);
  // `browser` should never be null here. The ComposeSession is indirectly owned
  // by the same WebContents that holds the field that the Compose dialog is
  // triggered from. The session is created when that dialog is opened and it is
  // destroyed if its WebContents is destroyed.
  CHECK(browser);
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupPageContentSubPage);
}

void ComposeSession::SaveMostRecentOkStateToUndoStack() {
  if (!most_recent_ok_state_->IsMojoValid()) {
    // Attempting to save a state with an invalid response onto the undo stack.
    return;
  }
  undo_states_.push(std::make_unique<ComposeState>(
      most_recent_ok_state_->TakeModelingLogEntry(),
      most_recent_ok_state_->TakeMojoState()));
}

void ComposeSession::AddPageContentToSession(const std::string& inner_text) {
  if (!session_) {
    return;
  }
  optimization_guide::proto::ComposePageMetadata page_metadata;
  page_metadata.set_page_url(web_contents_->GetLastCommittedURL().spec());
  page_metadata.set_page_title(base::UTF16ToUTF8(web_contents_->GetTitle()));
  page_metadata.set_page_inner_text(inner_text);

  optimization_guide::proto::ComposeRequest request;
  *request.mutable_page_metadata() = std::move(page_metadata);

  session_->AddContext(request);
}

void ComposeSession::UpdateInnerTextAndContinueComposeIfNecessary(
    const std::string& inner_text) {
  inner_text_ = inner_text;
  AddPageContentToSession(inner_text);
  if (!continue_compose_.is_null()) {
    std::move(continue_compose_).Run();
  }
}

void ComposeSession::RefreshInnerText() {
  inner_text_ = std::nullopt;
  if (skip_inner_text_) {
    return;
  }

  inner_text_extractor_.Extract(
      web_contents_,
      base::BindOnce(
          &ComposeSession::UpdateInnerTextAndContinueComposeIfNecessary,
          weak_ptr_factory_.GetWeakPtr()));
}

void ComposeSession::SetCloseReason(
    compose::ComposeSessionCloseReason close_reason) {
  close_reason_ = close_reason;
  switch (close_reason) {
    case compose::ComposeSessionCloseReason::kCloseButtonPressed:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_ABANDONED;
      break;
    case compose::ComposeSessionCloseReason::kEndedImplicitly:
      final_status_ = optimization_guide::proto::FinalStatus::
          STATUS_FINISHED_WITHOUT_INSERT;
      break;
    case compose::ComposeSessionCloseReason::kAcceptedSuggestion:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_INSERTED;
      break;
    default:
      final_status_ =
          optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED;
      break;
  }
}

void ComposeSession::SendQualityLogEntryUponError(
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
    base::TimeDelta request_time) {
  if (log_entry && model_quality_logs_uploader_) {
    log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_request_latency_ms(request_time.InMilliseconds());
    model_quality_logs_uploader_->UploadModelQualityLogs(std::move(log_entry));
  }
}
