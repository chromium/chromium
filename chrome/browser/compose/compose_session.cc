// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/content_extraction/inner_text.h"
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
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/compose_utils.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/network_service_instance.h"
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

  return compose::IsWordCountWithinBounds(prompt, config.input_min_words,
                                          config.input_max_words);
}

const char kComposeBugReportURL[] = "https://goto.google.com/ccbrfd";
const char kOnDeviceComposeBugReportURL[] = "https://goto.google.com/ccbrfdod";
const char kComposeLearnMorePageURL[] =
    "https://support.google.com/chrome?p=help_me_write";
const char kComposeFeedbackSurveyURL[] = "https://goto.google.com/ccfsfd";
const char kSignInPageURL[] = "https://accounts.google.com";
const char kOnDeviceComposeFeedbackSurveyURL[] =
    "https://goto.google.com/ccfsfdod";

compose::EvalLocation GetEvalLocation(
    const optimization_guide::OptimizationGuideModelStreamingExecutionResult&
        result) {
  return result.provided_by_on_device ? compose::EvalLocation::kOnDevice
                                      : compose::EvalLocation::kServer;
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

  void UploadModelQualityLogs(
      raw_ptr<optimization_guide::ModelQualityLogsUploader> logs_uploader) {
    if (!logs_uploader || !modeling_log_entry_) {
      return;
    }
    LogRequestFeedback();
    logs_uploader->UploadModelQualityLogs(TakeModelingLogEntry());
  }

  void LogRequestFeedback() {
    if (!mojo_state_ || !mojo_state_->response) {
      // No request or modeling information so nothing to report.
      return;
    }
    if (mojo_state_->response->status != compose::mojom::ComposeStatus::kOk) {
      // Request Feedback was already reported when error was received.
      return;
    }

    compose::EvalLocation eval_location =
        mojo_state_->response->on_device_evaluation_used
            ? compose::EvalLocation::kOnDevice
            : compose::EvalLocation::kServer;
    compose::ComposeRequestFeedback feedback;
    switch (mojo_state_->feedback) {
      case compose::mojom::UserFeedback::kUserFeedbackPositive:
        feedback = compose::ComposeRequestFeedback::kPositiveFeedback;
        break;
      case compose::mojom::UserFeedback::kUserFeedbackNegative:
        feedback = compose::ComposeRequestFeedback::kNegativeFeedback;
        break;
      case compose::mojom::UserFeedback::kUserFeedbackUnspecified:
        feedback = compose::ComposeRequestFeedback::kNoFeedback;
        break;
    }
    compose::LogComposeRequestFeedback(eval_location, feedback);
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
    InnerTextProvider* inner_text,
    autofill::FieldRendererId node_id,
    ComposeCallback callback)
    : executor_(executor),
      handler_receiver_(this),
      current_msbb_state_(false),
      msbb_initially_off_(false),
      msbb_close_reason_(
          compose::ComposeMSBBSessionCloseReason::kMSBBEndedImplicitly),
      fre_close_reason_(
          compose::ComposeFirstRunSessionCloseReason::kEndedImplicitly),
      close_reason_(compose::ComposeSessionCloseReason::kEndedImplicitly),
      final_status_(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED),
      web_contents_(web_contents),
      collect_inner_text_(
          base::FeatureList::IsEnabled(compose::features::kComposeInnerText)),
      inner_text_caller_(inner_text),
      ukm_source_id_(web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()),
      node_id_(node_id),
      model_quality_logs_uploader_(model_quality_logs_uploader),
      session_id_(session_id),
      weak_ptr_factory_(this) {
  session_duration_ = std::make_unique<base::ElapsedTimer>();
  callback_ = std::move(callback);
  current_state_ = compose::mojom::ComposeState::New();
  most_recent_ok_state_ = std::make_unique<ComposeState>();
  if (executor_) {
    session_ = executor_->StartSession(
        optimization_guide::proto::ModelExecutionFeature::
            MODEL_EXECUTION_FEATURE_COMPOSE,
        /*config_params=*/std::nullopt);
  }
}

ComposeSession::~ComposeSession() {
  std::optional<compose::EvalLocation> eval_location =
      compose::GetEvalLocationFromEvents(session_events_);

  if (session_events_.fre_dialog_shown_count > 0 &&
      (!fre_complete_ || session_events_.fre_completed_in_session)) {
    compose::LogComposeFirstRunSessionCloseReason(fre_close_reason_);
    compose::LogComposeFirstRunSessionDialogShownCount(
        fre_close_reason_, session_events_.fre_dialog_shown_count);
    if (!fre_complete_) {
      compose::LogComposeSessionDuration(session_duration_->Elapsed(), ".FRE");
      return;
    }
  }
  if (session_events_.msbb_dialog_shown_count > 0 &&
      (!current_msbb_state_ || session_events_.msbb_enabled_in_session)) {
    compose::LogComposeMSBBSessionDialogShownCount(
        msbb_close_reason_, session_events_.msbb_dialog_shown_count);
    compose::LogComposeMSBBSessionCloseReason(msbb_close_reason_);
    if (!current_msbb_state_) {
      compose::LogComposeSessionDuration(session_duration_->Elapsed(), ".MSBB");
      return;
    }
  }

  if (session_events_.dialog_shown_count < 1) {
    // Do not report any further metrics if the dialog was never shown.
    // This is mostly like because the session was the debug session but
    // could occur if the tab closes while Compose is opening.
    return;
  }

  if (session_events_.inserted_results) {
    compose::LogComposeSessionDuration(session_duration_->Elapsed(),
                                       ".Inserted", eval_location);
  } else {
    compose::LogComposeSessionDuration(session_duration_->Elapsed(), ".Ignored",
                                       eval_location);
  }
  if (close_reason_ == compose::ComposeSessionCloseReason::kEndedImplicitly) {
    base::RecordAction(
        base::UserMetricsAction("Compose.EndedSession.EndedImplicitly"));

    final_status_ =
        optimization_guide::proto::FinalStatus::STATUS_FINISHED_WITHOUT_INSERT;
  }

  LogComposeSessionCloseMetrics(close_reason_, session_events_);

  LogComposeSessionCloseUkmMetrics(ukm_source_id_, session_events_);

  // Quality log would automatically be uploaded on the destruction of
  // a modeling_log_entry. However in order to more easily test the quality
  // uploads we are calling upload directly here.

  if (!model_quality_logs_uploader_) {
    // Can not upload any logs so exit early.
    return;
  }

  if (most_recent_error_log_) {
    // First set final status on most_recent_error_log
    most_recent_error_log_
        ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_final_status(final_status_);
    model_quality_logs_uploader_->UploadModelQualityLogs(
        std::move(most_recent_error_log_));
  } else if (most_recent_ok_state_->modeling_log_entry()) {
    // First set final status on most_recent_ok_state_.
    most_recent_ok_state_->modeling_log_entry()
        ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_final_status(final_status_);
    most_recent_ok_state_->UploadModelQualityLogs(model_quality_logs_uploader_);
  }

  // Explicitly upload the rest of the undo stack.
  while (!undo_states_.empty()) {
    undo_states_.top()->UploadModelQualityLogs(model_quality_logs_uploader_);
    undo_states_.pop();
  }
}

void ComposeSession::Bind(
    mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
        handler,
    mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog) {
  handler_receiver_.reset();
  handler_receiver_.Bind(std::move(handler));

  dialog_remote_.reset();
  dialog_remote_.Bind(std::move(dialog));
}

// TODO(b/f3213db859d47): Add histogram test for Sessions triggering CancelEdit.
void ComposeSession::LogCancelEdit() {
  session_events_.did_click_cancel_on_edit = true;
}

// ComposeSessionUntrustedPageHandler
void ComposeSession::Compose(const std::string& input, bool is_input_edited) {
  compose::ComposeRequestReason request_reason;
  if (is_input_edited) {
    session_events_.update_input_count += 1;
    request_reason = compose::ComposeRequestReason::kUpdateRequest;
  } else {
    base::RecordAction(
        base::UserMetricsAction("Compose.ComposeRequest.CreateClicked"));
    request_reason = compose::ComposeRequestReason::kFirstRequest;
  }
  optimization_guide::proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input(input);
  MakeRequest(std::move(request), request_reason, is_input_edited);
}

void ComposeSession::Rewrite(compose::mojom::StyleModifiersPtr style) {
  compose::ComposeRequestReason request_reason;

  optimization_guide::proto::ComposeRequest request;
  if (style && style->is_tone()) {
    request.mutable_rewrite_params()->set_tone(
        optimization_guide::proto::ComposeTone(style->get_tone()));
    if (style->get_tone() == compose::mojom::Tone::kFormal) {
      session_events_.formal_count++;
      request_reason = compose::ComposeRequestReason::kToneFormalRequest;
    } else {
      session_events_.casual_count++;
      request_reason = compose::ComposeRequestReason::kToneCasualRequest;
    }
  } else if (style && style->is_length()) {
    request.mutable_rewrite_params()->set_length(
        optimization_guide::proto::ComposeLength(style->get_length()));
    if (style->get_length() == compose::mojom::Length::kLonger) {
      session_events_.lengthen_count++;
      request_reason = compose::ComposeRequestReason::kLengthElaborateRequest;
    } else {
      session_events_.shorten_count++;
      request_reason = compose::ComposeRequestReason::kLengthShortenRequest;
    }
  } else {
    request.mutable_rewrite_params()->set_regenerate(true);
    session_events_.regenerate_count++;
    request_reason = compose::ComposeRequestReason::kRetryRequest;
  }
  request.mutable_rewrite_params()->set_previous_response(
      most_recent_ok_state_->mojo_state()->response->result);
  MakeRequest(std::move(request), request_reason, false);
}

// TODO(b/300974056): Add histogram test for Sessions triggering EditInput.
void ComposeSession::LogEditInput() {
  session_events_.did_click_edit = true;
}

void ComposeSession::MakeRequest(
    optimization_guide::proto::ComposeRequest request,
    compose::ComposeRequestReason request_reason,
    bool is_input_edited) {
  current_state_->has_pending_request = true;
  current_state_->feedback =
      compose::mojom::UserFeedback::kUserFeedbackUnspecified;
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!session_ ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    ProcessError(compose::EvalLocation::kServer,
                 compose::mojom::ComposeStatus::kMisconfiguration);
    return;
  }

  // Increase compose count regardless of status of request.
  session_events_.compose_count += 1;

  if (!collect_inner_text_ || got_inner_text_) {
    RequestWithSession(std::move(request), request_reason, is_input_edited);
  } else {
    // Prepare the compose call, which will be invoked when inner text
    // extraction is completed.
    continue_compose_ = base::BindOnce(
        &ComposeSession::RequestWithSession, weak_ptr_factory_.GetWeakPtr(),
        std::move(request), request_reason, is_input_edited);
  }
}

void ComposeSession::RequestWithSession(
    const optimization_guide::proto::ComposeRequest& request,
    compose::ComposeRequestReason request_reason,
    bool is_input_edited) {
  if (!collect_inner_text_) {
    // Make sure context is added for sessions with no inner text.
    AddPageContentToSession("", std::nullopt);
  }

  // Add timeout for high latency Compose requests.
  const compose::Config& config = compose::GetComposeConfig();

  base::ElapsedTimer request_timer;
  request_id_++;

  auto timeout = std::make_unique<base::OneShotTimer>();
  timeout->Start(FROM_HERE,
                 base::Seconds(config.request_latency_timeout_seconds),
                 base::BindOnce(&ComposeSession::ComposeRequestTimeout,
                                base::Unretained(this), request_id_));
  request_timeouts_.emplace(request_id_, std::move(timeout));

  // Record the eval_location independent request metrics before model
  // execution in case request fails.
  compose::LogComposeRequestReason(request_reason);

  session_->ExecuteModel(
      request, base::BindRepeating(&ComposeSession::ModelExecutionCallback,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(request_timer), request_id_,
                                   request_reason, is_input_edited));
}

void ComposeSession::ComposeRequestTimeout(int id) {
  request_timeouts_.erase(id);
  compose::LogComposeRequestStatus(
      compose::mojom::ComposeStatus::kRequestTimeout);

  current_state_->has_pending_request = false;
  current_state_->response = compose::mojom::ComposeResponse::New();
  current_state_->response->status =
      compose::mojom::ComposeStatus::kRequestTimeout;

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(current_state_->response->Clone());
  }
}

void ComposeSession::ModelExecutionCallback(
    const base::ElapsedTimer& request_timer,
    int request_id,
    compose::ComposeRequestReason request_reason,
    bool was_input_edited,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  base::TimeDelta request_delta = request_timer.Elapsed();

  compose::EvalLocation eval_location = GetEvalLocation(result);

  // Presence of the timer with the corresponding `request_id` indicates that
  // the request has not timed out - process the response. Otherwise ignore the
  // response.
  if (auto iter = request_timeouts_.find(request_id);
      iter != request_timeouts_.end()) {
    iter->second->Stop();
    // If a partial response was received, then this callback may be reused.
    // Only remove the associated timer if the response is complete.
    if (result.response.has_value() && result.response->is_complete) {
      request_timeouts_.erase(request_id);
    }
  } else {
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);

    compose::LogComposeRequestReason(eval_location, request_reason);
    compose::LogComposeRequestStatus(
        eval_location, compose::mojom::ComposeStatus::kRequestTimeout);
    return;
  }

  // A new request has been issued, ignore this one.
  if (request_id != request_id_) {
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);
    compose::LogComposeRequestReason(eval_location, request_reason);
    return;
  }

  if (result.response.has_value() && !result.response->is_complete) {
    ModelExecutionProgress(std::move(result.response).value());
    return;
  }

  ModelExecutionComplete(request_delta, request_reason, was_input_edited,
                         std::move(result));
}

void ComposeSession::ModelExecutionProgress(
    optimization_guide::StreamingResponse result) {
  CHECK(base::FeatureList::IsEnabled(
      optimization_guide::features::kOptimizationGuideOnDeviceModel));
  if (!base::FeatureList::IsEnabled(
          compose::features::kComposeTextOutputAnimation)) {
    return;
  }
  if (!dialog_remote_.is_bound()) {
    return;
  }
  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result.response);
  if (!response) {
    DLOG(ERROR) << "Failed to parse partial compose response";
    return;
  }
  auto partial_ui_response = compose::mojom::PartialComposeResponse::New();
  partial_ui_response->result = response->output();
  dialog_remote_->PartialResponseReceived(std::move(partial_ui_response));
}

void ComposeSession::ModelExecutionComplete(
    base::TimeDelta request_delta,
    compose::ComposeRequestReason request_reason,
    bool was_input_edited,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  // Handle 'complete' results.
  current_state_->has_pending_request = false;
  compose::EvalLocation eval_location = GetEvalLocation(result);
  if (eval_location == compose::EvalLocation::kOnDevice) {
    ++session_events_.on_device_responses;
  } else {
    ++session_events_.server_responses;
  }

  compose::LogComposeRequestReason(eval_location, request_reason);

  compose::mojom::ComposeStatus status =
      ComposeStatusFromOptimizationGuideResult(result);

  if (status != compose::mojom::ComposeStatus::kOk) {
    compose::LogComposeRequestDuration(request_delta, eval_location,
                                       /* is_ok */ false);
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      ProcessError(eval_location, compose::mojom::ComposeStatus::kOffline);
    } else {
      ProcessError(eval_location, status);
    }
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);
    return;
  }
  DCHECK(result.response->is_complete);

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result.response->response);

  if (!response) {
    compose::LogComposeRequestDuration(request_delta, eval_location,
                                       /* is_ok */ false);
    ProcessError(eval_location, compose::mojom::ComposeStatus::kNoResponse);
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);
    return;
  }

  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();
  ui_response->on_device_evaluation_used = result.provided_by_on_device;
  current_state_->response = ui_response->Clone();

  // Log successful response status.
  compose::LogComposeRequestStatus(compose::mojom::ComposeStatus::kOk);
  compose::LogComposeRequestStatus(eval_location,
                                   compose::mojom::ComposeStatus::kOk);
  compose::LogComposeRequestDuration(request_delta, eval_location,
                                     /* is_ok */ true);

  SaveMostRecentOkStateToUndoStack();
  most_recent_ok_state_->SetMojoState(current_state_->Clone());

  ui_response->undo_available = !undo_states_.empty();
  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(std::move(ui_response));
  }

  if (result.log_entry) {
    result.log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_was_generated_via_edit(was_input_edited);
    result.log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_request_latency_ms(request_delta.InMilliseconds());
    optimization_guide::proto::Int128* token =
        result.log_entry
            ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
            ->mutable_session_id();

    token->set_high(session_id_.high());
    token->set_low(session_id_.low());
    most_recent_ok_state_->SetModelingLogEntry(std::move(result.log_entry));
    // In the event that we are holding onto an error log upload it before it
    // gets overwritten
    if (most_recent_error_log_ && model_quality_logs_uploader_) {
      model_quality_logs_uploader_->UploadModelQualityLogs(
          std::move(most_recent_error_log_));
    }

    // if we have a valid most recent state we no longer need an error state.
    most_recent_error_log_.reset();
  }
}

void ComposeSession::ProcessError(compose::EvalLocation eval_location,
                                  compose::mojom::ComposeStatus error) {
  compose::LogComposeRequestStatus(error);
  compose::LogComposeRequestStatus(eval_location, error);

  // Feedback can not be given for a request with an error so report now.
  compose::LogComposeRequestFeedback(
      eval_location, compose::ComposeRequestFeedback::kRequestError);

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
      fre_complete_, current_msbb_state_, initial_input_, text_selected_,
      current_state_->Clone(),
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
  session_events_.undo_count += 1;

  std::unique_ptr<ComposeState> undo_state = std::move(undo_states_.top());
  undo_states_.pop();

  // upload the most recent modeling quality log entry before overwriting it
  // with state from undo,

  most_recent_ok_state_->UploadModelQualityLogs(model_quality_logs_uploader_);

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
  const char* url = kComposeBugReportURL;
  if (most_recent_ok_state_ && most_recent_ok_state_->mojo_state() &&
      most_recent_ok_state_->mojo_state()
          ->response->on_device_evaluation_used) {
    url = kOnDeviceComposeBugReportURL;
  }
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(url), content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenComposeLearnMorePage() {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(kComposeLearnMorePageURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenFeedbackSurveyLink() {
  const char* url = kComposeFeedbackSurveyURL;
  if (most_recent_ok_state_ && most_recent_ok_state_->mojo_state() &&
      most_recent_ok_state_->mojo_state()
          ->response->on_device_evaluation_used) {
    url = kOnDeviceComposeFeedbackSurveyURL;
  }
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(url), content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenSignInPage() {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(kSignInPageURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::OpenFeedbackPage(std::string feedback_id) {
  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", feedback_id);

  if (allow_feedback_for_testing_) {
    return;
  }

  chrome::ShowFeedbackPage(
      web_contents_->GetLastCommittedURL(),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      chrome::kFeedbackSourceAI,
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

  // TODO(b/314199871): Remove test bypass once this check becomes mock-able.
  if (!allow_feedback_for_testing_) {
    OptimizationGuideKeyedService* opt_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
    if (!opt_guide_keyed_service ||
        !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForLogging(
            optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE)) {
      return;
    }
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
                                    ->model_execution_info()
                                    .execution_id();
      session_events_.has_thumbs_down = true;
      OpenFeedbackPage(feedback_id);
    } else if (feedback ==
               compose::mojom::UserFeedback::kUserFeedbackPositive) {
      session_events_.has_thumbs_up = true;
    }
  }
}

void ComposeSession::InitializeWithText(const std::optional<std::string>& text,
                                        const bool text_selected) {
  // In some cases (FRE not shown, MSBB not accepted), we wait to extract the
  // inner text until all conditions are met to enable the feature.  However, if
  // we want to extract the inner text content later, we still need to store the
  // selected text.
  text_selected_ = text_selected;
  if (text.has_value()) {
    initial_input_ = text.value();
    session_events_.has_initial_text = true;
  }

  if (!fre_complete_) {
    session_events_.fre_dialog_shown_count += 1;
    return;
  }
  if (!current_msbb_state_) {
    session_events_.msbb_dialog_shown_count += 1;
    return;
  }

  // Session is initialized at the main dialog UI state.
  session_events_.dialog_shown_count += 1;

  RefreshInnerText();

  // If no text provided (even an empty string), then we are reopening without
  // calling compose again, or updating the input text, so skip autocompose.
  if (text.has_value() && IsValidComposePrompt(initial_input_) &&
      compose::GetComposeConfig().auto_submit_with_selection) {
    Compose(initial_input_, false);
  }
}

void ComposeSession::SaveMostRecentOkStateToUndoStack() {
  if (!most_recent_ok_state_->IsMojoValid()) {
    // This occurs when processing the first ok response of a session - no
    // previous ok state exists and so there is nothing to save to the undo
    // stack.
    return;
  }
  undo_states_.push(std::make_unique<ComposeState>(
      most_recent_ok_state_->TakeModelingLogEntry(),
      most_recent_ok_state_->TakeMojoState()));
}

void ComposeSession::AddPageContentToSession(
    std::string inner_text,
    std::optional<uint64_t> node_offset) {
  if (!session_) {
    return;
  }
  optimization_guide::proto::ComposePageMetadata page_metadata;
  page_metadata.set_page_url(web_contents_->GetLastCommittedURL().spec());
  page_metadata.set_page_title(base::UTF16ToUTF8(web_contents_->GetTitle()));
  page_metadata.set_page_inner_text(std::move(inner_text));

  if (node_offset.has_value()) {
    page_metadata.set_page_inner_text_offset(node_offset.value());
  }

  optimization_guide::proto::ComposeRequest request;
  *request.mutable_page_metadata() = std::move(page_metadata);

  session_->AddContext(request);
}

void ComposeSession::UpdateInnerTextAndContinueComposeIfNecessary(
    int request_id,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (request_id != current_inner_text_request_id_) {
    // If this condition is hit, it means there are multiple requests for
    // inner-text in flight. Early out so that we always use the most recent
    // request.
    return;
  }
  got_inner_text_ = true;
  std::string inner_text;
  std::optional<uint64_t> node_offset;
  if (result) {
    const compose::Config& config = compose::GetComposeConfig();
    inner_text = std::move(result->inner_text);
    compose::LogComposeDialogInnerTextSize(inner_text.size());
    if (inner_text.size() > config.inner_text_max_bytes) {
      compose::LogComposeDialogInnerTextShortenedBy(
          inner_text.size() - config.inner_text_max_bytes);
      inner_text.erase(config.inner_text_max_bytes);
    }
    node_offset = result->node_offset;
    compose::LogComposeDialogInnerTextOffsetFound(node_offset.has_value());
  }
  AddPageContentToSession(std::move(inner_text), node_offset);
  if (!continue_compose_.is_null()) {
    std::move(continue_compose_).Run();
  }
}

void ComposeSession::RefreshInnerText() {
  got_inner_text_ = false;
  if (!collect_inner_text_) {
    return;
  }

  ++current_inner_text_request_id_;

  inner_text_caller_->GetInnerText(
      *web_contents_->GetPrimaryMainFrame(),
      // This unsafeValue call is acceptable ehre because node_id is a
      // FieldRendererId which while being an U64 type is based one the int
      // DOMid which we are querying here.
      node_id_.GetUnsafeValue(),
      base::BindOnce(
          &ComposeSession::UpdateInnerTextAndContinueComposeIfNecessary,
          weak_ptr_factory_.GetWeakPtr(), current_inner_text_request_id_));
}

void ComposeSession::SetFirstRunCloseReason(
    compose::ComposeFirstRunSessionCloseReason close_reason) {
  fre_close_reason_ = close_reason;

  if (close_reason == compose::ComposeFirstRunSessionCloseReason::
                          kFirstRunDisclaimerAcknowledgedWithoutInsert) {
    if (current_msbb_state_) {
      // The FRE dialog progresses directly to the main dialog.
      session_events_.dialog_shown_count = 1;
      base::RecordAction(
          base::UserMetricsAction("Compose.DialogSeen.MainDialog"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("Compose.DialogSeen.FirstRunMSBB"));
    }
  }
}

void ComposeSession::SetFirstRunCompleted() {
  session_events_.fre_completed_in_session = true;
  fre_complete_ = true;

  // Start inner text capture which was skipped until FRE was complete.
  InitializeWithText(std::make_optional(initial_input_), text_selected_);
}

void ComposeSession::SetMSBBCloseReason(
    compose::ComposeMSBBSessionCloseReason close_reason) {
  msbb_close_reason_ = close_reason;
}

void ComposeSession::SetCloseReason(
    compose::ComposeSessionCloseReason close_reason) {
  if (close_reason == compose::ComposeSessionCloseReason::kCloseButtonPressed &&
      current_state_->has_pending_request) {
    close_reason_ =
        compose::ComposeSessionCloseReason::kCanceledBeforeResponseReceived;
  } else {
    close_reason_ = close_reason;
  }

  switch (close_reason) {
    case compose::ComposeSessionCloseReason::kCloseButtonPressed:
    case compose::ComposeSessionCloseReason::kNewSessionWithSelectedText:
    case compose::ComposeSessionCloseReason::kCanceledBeforeResponseReceived:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_ABANDONED;
      session_events_.close_clicked = true;
      break;
    case compose::ComposeSessionCloseReason::kEndedImplicitly:
      final_status_ = optimization_guide::proto::FinalStatus::
          STATUS_FINISHED_WITHOUT_INSERT;
      break;
    case compose::ComposeSessionCloseReason::kAcceptedSuggestion:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_INSERTED;
      session_events_.inserted_results = true;
      break;
  }
}

void ComposeSession::SetQualityLogEntryUponError(
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
    base::TimeDelta request_time,
    bool was_input_edited) {
  if (log_entry) {
    log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_request_latency_ms(request_time.InMilliseconds());
    optimization_guide::proto::Int128* token =
        log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
            ->mutable_session_id();

    token->set_high(session_id_.high());
    token->set_low(session_id_.low());

    log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_was_generated_via_edit(was_input_edited);
    // In the event that we are holding onto an error log upload it before it
    // gets overwritten
    if (most_recent_error_log_ && model_quality_logs_uploader_) {
      model_quality_logs_uploader_->UploadModelQualityLogs(
          std::move(most_recent_error_log_));
    }

    most_recent_error_log_ = std::move(log_entry);
  }
}

void ComposeSession::set_current_msbb_state(bool msbb_enabled) {
  current_msbb_state_ = msbb_enabled;
  if (!msbb_enabled) {
    msbb_initially_off_ = true;
  } else if (msbb_initially_off_) {
    session_events_.msbb_enabled_in_session = true;
    SetMSBBCloseReason(
        compose::ComposeMSBBSessionCloseReason::kMSBBAcceptedWithoutInsert);
    base::RecordAction(
        base::UserMetricsAction("Compose.DialogSeen.MainDialog"));

    // Reset this initial state so that this block is not re-executed on every
    // subsequent dialog open.
    msbb_initially_off_ = false;
  }
}

void ComposeSession::SetAllowFeedbackForTesting(bool allowed) {
  allow_feedback_for_testing_ = allowed;
}
