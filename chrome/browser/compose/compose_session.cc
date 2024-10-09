// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/compose/type_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_hats_utils.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/compose_utils.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"
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

compose::ComposeRequestReason GetRequestReasonForInputMode(
    compose::mojom::InputMode mode) {
  switch (mode) {
    case compose::mojom::InputMode::kElaborate:
      return compose::ComposeRequestReason::kFirstRequestElaborateMode;
    case compose::mojom::InputMode::kFormalize:
      return compose::ComposeRequestReason::kFirstRequestFormalizeMode;
    case compose::mojom::InputMode::kPolish:
      return compose::ComposeRequestReason::kFirstRequestPolishMode;
    case compose::mojom::InputMode::kUnset:
      return compose::ComposeRequestReason::kFirstRequest;
  }
}

bool WasRequestTriggeredFromModifier(compose::ComposeRequestReason reason) {
  switch (reason) {
    case compose::ComposeRequestReason::kRetryRequest:
    case compose::ComposeRequestReason::kLengthShortenRequest:
    case compose::ComposeRequestReason::kLengthElaborateRequest:
    case compose::ComposeRequestReason::kToneCasualRequest:
    case compose::ComposeRequestReason::kToneFormalRequest:
      return true;
    case compose::ComposeRequestReason::kUpdateRequest:
    case compose::ComposeRequestReason::kFirstRequest:
    case compose::ComposeRequestReason::kFirstRequestPolishMode:
    case compose::ComposeRequestReason::kFirstRequestElaborateMode:
    case compose::ComposeRequestReason::kFirstRequestFormalizeMode:
      return false;
  }
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

  bool is_user_edited() { return is_user_edited_; }
  std::string original_response() { return original_response_; }

  void SetUserEdited(std::string original_response) {
    original_response_ = original_response;
    is_user_edited_ = true;
  }

  void UnsetUserEdited() {
    original_response_ = "";
    is_user_edited_ = false;
  }

  void SetMojoState(compose::mojom::ComposeStatePtr mojo_state) {
    mojo_state_ = std::move(mojo_state);
  }

  void UploadModelQualityLogs() {
    if (!modeling_log_entry_) {
      return;
    }
    LogRequestFeedback();
    optimization_guide::ModelQualityLogEntry::Upload(TakeModelingLogEntry());
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
  std::string original_response_ = "";
  bool is_user_edited_ = false;
};

ComposeSession::ComposeSession(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideModelExecutor* executor,
    base::Token session_id,
    InnerTextProvider* inner_text,
    autofill::FieldGlobalId node_id,
    bool is_page_language_supported,
    Observer* observer,
    ComposeCallback callback)
    : executor_(executor),
      handler_receiver_(this),
      web_contents_(web_contents),
      observer_(observer),
      collect_inner_text_(
          base::FeatureList::IsEnabled(compose::features::kComposeInnerText)),
      collect_ax_snapshot_(
          base::FeatureList::IsEnabled(compose::features::kComposeAXSnapshot)),
      inner_text_caller_(inner_text),
      ukm_source_id_(web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()),
      node_id_(node_id),
      is_page_language_supported_(is_page_language_supported),
      session_id_(session_id),
      weak_ptr_factory_(this) {
  session_duration_ = std::make_unique<base::ElapsedTimer>();
  callback_ = std::move(callback);
  active_mojo_state_ = compose::mojom::ComposeState::New();
  if (executor_) {
    optimization_guide::SessionConfigParams config_params = {
        .execution_mode = base::FeatureList::IsEnabled(
                              compose::features::kComposeAllowOnDeviceExecution)
                              ? optimization_guide::SessionConfigParams::
                                    ExecutionMode::kDefault
                              : optimization_guide::SessionConfigParams::
                                    ExecutionMode::kServerOnly};
    session_ = executor_->StartSession(
        optimization_guide::ModelBasedCapabilityKey::kCompose, config_params);
  }
}

base::optional_ref<ComposeState> ComposeSession::LastResponseState() {
  if (history_.empty() || history_current_index_ >= history_.size() ||
      !history_[history_current_index_]) {
    return std::nullopt;
  }

  if (history_[history_current_index_]->is_user_edited()) {
    // The state at `history_current_index_` is an edited result, so the last
    // response state directly precedes it in `history_`.
    CHECK_GT(history_current_index_, 0u);
    return *history_[history_current_index_ - 1];
  }

  return *history_[history_current_index_];
}

base::optional_ref<ComposeState> ComposeSession::CurrentState(int offset) {
  if (history_.empty() || history_current_index_ + offset >= history_.size() ||
      !history_[history_current_index_ + offset]) {
    return std::nullopt;
  }

  return *history_[history_current_index_ + offset];
}

ComposeSession::~ComposeSession() {
  std::optional<compose::EvalLocation> eval_location =
      compose::GetEvalLocationFromEvents(session_events_);

  if (observer_) {
    observer_->OnSessionComplete(node_id_, close_reason_, session_events_);
  }

  if (session_events_.fre_view_count > 0 &&
      (!fre_complete_ || session_events_.fre_completed_in_session)) {
    compose::LogComposeFirstRunSessionCloseReason(fre_close_reason_);
    compose::LogComposeFirstRunSessionDialogShownCount(
        fre_close_reason_, session_events_.fre_view_count);
    if (!fre_complete_) {
      compose::LogComposeSessionDuration(session_duration_->Elapsed(), ".FRE");
      compose::LogComposeSessionEventCounts(std::nullopt, session_events_);
      compose::LogComposeSessionCloseReason(
          compose::ComposeSessionCloseReason::kEndedAtFre);
      return;
    }
  }
  if (session_events_.msbb_view_count > 0 &&
      (!current_msbb_state_ || session_events_.msbb_enabled_in_session)) {
    compose::LogComposeMSBBSessionDialogShownCount(
        msbb_close_reason_, session_events_.msbb_view_count);
    compose::LogComposeMSBBSessionCloseReason(msbb_close_reason_);
    if (!current_msbb_state_) {
      compose::LogComposeSessionDuration(session_duration_->Elapsed(), ".MSBB");
      compose::LogComposeSessionEventCounts(std::nullopt, session_events_);
      compose::ComposeSessionCloseReason session_close_reason =
          (session_events_.fre_completed_in_session)
              ? compose::ComposeSessionCloseReason::kAckedFreEndedAtMsbb
              : compose::ComposeSessionCloseReason::kEndedAtMsbb;
      compose::LogComposeSessionCloseReason(session_close_reason);
      return;
    }
  }

  if (session_events_.compose_dialog_open_count < 1) {
    // Do not report any further metrics if the dialog was never opened.
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
  if (close_reason_ == compose::ComposeSessionCloseReason::kAbandoned) {
    base::RecordAction(
        base::UserMetricsAction("Compose.EndedSession.EndedImplicitly"));
    final_model_status_ =
        optimization_guide::proto::FinalModelStatus::FINAL_MODEL_STATUS_FAILURE;
    final_status_ =
        optimization_guide::proto::FinalStatus::STATUS_FINISHED_WITHOUT_INSERT;
  }

  LogComposeSessionCloseMetrics(close_reason_, session_events_);

  LogComposeSessionCloseUkmMetrics(ukm_source_id_, session_events_);

  // Quality log would automatically be uploaded on the destruction of
  // a modeling_log_entry. However in order to more easily test the quality
  // uploads we are calling upload directly here.

  if (most_recent_error_log_) {
    // First set final status on most_recent_error_log.
    most_recent_error_log_
        ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_final_status(final_status_);
    most_recent_error_log_
        ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_final_model_status(final_model_status_);

    optimization_guide::ModelQualityLogEntry::Upload(
        std::move(most_recent_error_log_));
  } else if (auto last_response_state = LastResponseState();
             last_response_state.has_value()) {
    if (auto* log_entry = last_response_state->modeling_log_entry()) {
      log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->set_final_status(final_status_);
      log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->set_final_model_status(final_model_status_);
      last_response_state->UploadModelQualityLogs();
    }
  }

  for (auto& state : history_) {
    // Upload all saved states with a valid quality logs member (those tied to
    // a ComposeResponse) and then clear all states.
    state->UploadModelQualityLogs();
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

// TODO(b/300974056): Add histogram test for Sessions triggering CancelEdit.
void ComposeSession::LogCancelEdit() {
  session_events_.did_click_cancel_on_edit = true;
}

// ComposeSessionUntrustedPageHandler
void ComposeSession::Compose(const std::string& input,
                             compose::mojom::InputMode mode,
                             bool is_input_edited) {
  compose::ComposeRequestReason request_reason;
  if (is_input_edited) {
    session_events_.update_input_count += 1;
    request_reason = compose::ComposeRequestReason::kUpdateRequest;
  } else {
    base::RecordAction(
        base::UserMetricsAction("Compose.ComposeRequest.CreateClicked"));
    request_reason = GetRequestReasonForInputMode(mode);
  }
  optimization_guide::proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input(input);
  optimization_guide::proto::ComposeUpfrontInputMode request_mode =
      ComposeUpfrontInputMode(mode);
  request.mutable_generate_params()->set_upfront_input_mode(request_mode);

  MakeRequest(std::move(request), request_reason, is_input_edited);
}

void ComposeSession::Rewrite(compose::mojom::StyleModifier style) {
  compose::ComposeRequestReason request_reason;

  optimization_guide::proto::ComposeRequest request;
  switch (style) {
    case compose::mojom::StyleModifier::kFormal:
      request.mutable_rewrite_params()->set_tone(
          optimization_guide::proto::ComposeTone::COMPOSE_FORMAL);
      session_events_.formal_count++;
      request_reason = compose::ComposeRequestReason::kToneFormalRequest;
      break;
    case compose::mojom::StyleModifier::kCasual:
      request.mutable_rewrite_params()->set_tone(
          optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL);
      session_events_.casual_count++;
      request_reason = compose::ComposeRequestReason::kToneCasualRequest;
      break;
    case compose::mojom::StyleModifier::kShorter:
      request.mutable_rewrite_params()->set_length(
          optimization_guide::proto::ComposeLength::COMPOSE_SHORTER);
      session_events_.shorten_count++;
      request_reason = compose::ComposeRequestReason::kLengthShortenRequest;
      break;
    case compose::mojom::StyleModifier::kLonger:
      request.mutable_rewrite_params()->set_length(
          optimization_guide::proto::ComposeLength::COMPOSE_LONGER);
      session_events_.lengthen_count++;
      request_reason = compose::ComposeRequestReason::kLengthElaborateRequest;
      break;
    case compose::mojom::StyleModifier::kUnset:
      // TODO: kUnset is not reachable, but a `request_reason` must be set to
      //  satisfy the compiler
    case compose::mojom::StyleModifier::kRetry:
      request.mutable_rewrite_params()->set_regenerate(true);
      session_events_.regenerate_count++;
      request_reason = compose::ComposeRequestReason::kRetryRequest;
      break;
  }
  request.mutable_rewrite_params()->set_previous_response(
      CurrentState()->mojo_state()->response->result);
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
  active_mojo_state_->has_pending_request = true;
  active_mojo_state_->feedback =
      compose::mojom::UserFeedback::kUserFeedbackUnspecified;

  // Increase Compose count regardless of status of request.
  ++session_events_.compose_requests_count;

  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!session_ ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    ProcessError(compose::EvalLocation::kServer,
                 compose::mojom::ComposeStatus::kMisconfiguration,
                 request_reason);
    return;
  }

  // Prepare the compose call, which will be invoked when all required page
  // metadata is collected.
  continue_compose_ = base::BindOnce(
      &ComposeSession::RequestWithSession, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), request_reason, is_input_edited);
  // In case AX tree or page collection isn't required, we can run the
  // continuation immediately. Note that going through this call ensures we
  // populate the context object correctly.
  TryContinueComposeWithContext();
}

bool ComposeSession::HasNecessaryPageContext() const {
  return (!collect_inner_text_ || got_inner_text_) &&
         (!collect_ax_snapshot_ || got_ax_snapshot_);
}

void ComposeSession::RequestWithSession(
    const optimization_guide::proto::ComposeRequest& request,
    compose::ComposeRequestReason request_reason,
    bool is_input_edited) {
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
      is_page_language_supported_,
      compose::mojom::ComposeStatus::kRequestTimeout);

  active_mojo_state_->has_pending_request = false;
  active_mojo_state_->response = compose::mojom::ComposeResponse::New();
  active_mojo_state_->response->status =
      compose::mojom::ComposeStatus::kRequestTimeout;

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(active_mojo_state_->response->Clone());
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
        eval_location, is_page_language_supported_,
        compose::mojom::ComposeStatus::kRequestTimeout);
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
  active_mojo_state_->has_pending_request = false;
  compose::EvalLocation eval_location = GetEvalLocation(result);
  if (eval_location == compose::EvalLocation::kOnDevice) {
    ++session_events_.on_device_responses;
  } else {
    ++session_events_.server_responses;
  }

  compose::LogComposeRequestReason(eval_location, request_reason);

  compose::mojom::ComposeStatus status =
      ComposeStatusFromOptimizationGuideResult(result);

  if (!session_events_.session_contained_filtered_response &&
      status == compose::mojom::ComposeStatus::kFiltered) {
    session_events_.session_contained_filtered_response = true;
  }
  if (!session_events_.session_contained_any_error &&
      status != compose::mojom::ComposeStatus::kOk) {
    session_events_.session_contained_any_error = true;
  }

  if (status != compose::mojom::ComposeStatus::kOk) {
    compose::LogComposeRequestDuration(request_delta, eval_location,
                                       /* is_ok */ false);
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      ProcessError(eval_location, compose::mojom::ComposeStatus::kOffline,
                   request_reason);
    } else {
      ProcessError(eval_location, status, request_reason);
    }
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);
    return;
  }
  CHECK(result.response->is_complete);

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result.response->response);

  if (!response) {
    compose::LogComposeRequestDuration(request_delta, eval_location,
                                       /* is_ok */ false);
    ProcessError(eval_location, compose::mojom::ComposeStatus::kNoResponse,
                 request_reason);
    SetQualityLogEntryUponError(std::move(result.log_entry), request_delta,
                                was_input_edited);
    return;
  }

  if (result.log_entry) {
    result.log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_was_generated_via_edit(was_input_edited);
    result.log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_started_with_proactive_nudge(
            session_events_.started_with_proactive_nudge);
    result.log_entry->quality_data<optimization_guide::ComposeFeatureTypeMap>()
        ->set_request_latency_ms(request_delta.InMilliseconds());
    optimization_guide::proto::Int128* token =
        result.log_entry
            ->quality_data<optimization_guide::ComposeFeatureTypeMap>()
            ->mutable_session_id();

    token->set_high(session_id_.high());
    token->set_low(session_id_.low());
    // In the event that we are holding onto an error log upload it before it
    // gets overwritten
    if (most_recent_error_log_) {
      optimization_guide::ModelQualityLogEntry::Upload(
          std::move(most_recent_error_log_));
    }

    // if we have a valid most recent state we no longer need an error state.
    most_recent_error_log_.reset();
  }

  // Create a new ComposeState with the dialog's current mojo state and the log
  // entry just received with the response.
  std::unique_ptr<ComposeState> new_response_state =
      std::make_unique<ComposeState>(std::move(result.log_entry),
                                     active_mojo_state_.Clone());
  // Update the new state's mojo state to reflect the new response.
  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();
  ui_response->on_device_evaluation_used = result.provided_by_on_device;
  ui_response->provided_by_user = false;
  // TODO(b/333944734): Remove undo_available and redo_available from
  // ComposeState.
  ui_response->undo_available = !history_.empty();
  ui_response->redo_available = false;
  new_response_state->mojo_state()->response = ui_response->Clone();

  // Before adding the new state to history, mark redo available on the current
  // state that will directly precede it.
  if (auto current_state = CurrentState(); current_state.has_value()) {
    current_state->mojo_state()->response->redo_available = true;
  }

  AddNewResponseToHistory(std::move(new_response_state));

  // Update `active_mojo_state_` to match the new state
  active_mojo_state_ = CurrentState()->mojo_state()->Clone();

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(active_mojo_state_->response->Clone());
  }

  // Log successful response status.
  compose::LogComposeRequestStatus(is_page_language_supported_,
                                   compose::mojom::ComposeStatus::kOk);
  compose::LogComposeRequestStatus(eval_location, is_page_language_supported_,
                                   compose::mojom::ComposeStatus::kOk);
  compose::LogComposeRequestDuration(request_delta, eval_location,
                                     /* is_ok */ true);
  ++session_events_.successful_requests_count;
}

void ComposeSession::AddNewResponseToHistory(
    std::unique_ptr<ComposeState> new_state) {
  // On a new response, all forward/redo states are cleared. Upload any
  // associated quality logs first.
  EraseForwardStatesInHistory();
  history_.push_back(std::move(new_state));
  history_current_index_ = history_.size() - 1;
}

void ComposeSession::EraseForwardStatesInHistory() {
  for (size_t i = history_current_index_ + 1; i < history_.size(); i++) {
    history_[i]->UploadModelQualityLogs();
  }
  if (history_.size() > history_current_index_ + 1) {
    history_.erase(history_.begin() + history_current_index_ + 1,
                   history_.end());
  }
}

void ComposeSession::ProcessError(
    compose::EvalLocation eval_location,
    compose::mojom::ComposeStatus error,
    compose::ComposeRequestReason request_reason) {
  compose::LogComposeRequestStatus(is_page_language_supported_, error);
  compose::LogComposeRequestStatus(eval_location, is_page_language_supported_,
                                   error);
  ++session_events_.failed_requests_count;

  // Feedback can not be given for a request with an error so report now.
  compose::LogComposeRequestFeedback(
      eval_location, compose::ComposeRequestFeedback::kRequestError);

  active_mojo_state_->has_pending_request = false;
  active_mojo_state_->response = compose::mojom::ComposeResponse::New();
  active_mojo_state_->response->status = error;
  active_mojo_state_->response->triggered_from_modifier =
      WasRequestTriggeredFromModifier(request_reason);

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(active_mojo_state_->response->Clone());
  }
}

void ComposeSession::RequestInitialState(RequestInitialStateCallback callback) {
  auto compose_config = compose::GetComposeConfig();

  std::move(callback).Run(compose::mojom::OpenMetadata::New(
      fre_complete_, current_msbb_state_, initial_input_,
      currently_has_selection_, active_mojo_state_->Clone(),
      compose::mojom::ConfigurableParams::New(compose_config.input_min_words,
                                              compose_config.input_max_words,
                                              compose_config.input_max_chars)));
}

void ComposeSession::SaveWebUIState(const std::string& webui_state) {
  active_mojo_state_->webui_state = webui_state;
}

void ComposeSession::AcceptComposeResult(
    AcceptComposeResultCallback success_callback) {
  if (callback_.is_null() || !active_mojo_state_->response ||
      active_mojo_state_->response->status !=
          compose::mojom::ComposeStatus::kOk) {
    // Guard against invoking twice before the UI is able to disconnect.
    std::move(success_callback).Run(false);
    return;
  }
  std::move(callback_).Run(
      base::UTF8ToUTF16(active_mojo_state_->response->result));
  std::move(success_callback).Run(true);
}

void ComposeSession::RecoverFromErrorState(
    RecoverFromErrorStateCallback callback) {
  // Should only be called if there is a state to return to.
  CHECK(CurrentState().has_value());
  if (!CurrentState()->IsMojoValid()) {
    // Gracefully fail if we find an invalid state.
    std::move(callback).Run(nullptr);
    return;
  }

  active_mojo_state_ = CurrentState()->mojo_state()->Clone();

  std::move(callback).Run(active_mojo_state_->Clone());
}

void ComposeSession::Undo(UndoCallback callback) {
  // Undo should only be called if a backwards saved state exists.
  if (history_current_index_ < 1) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto previous_state = CurrentState(-1);
  if (!previous_state->IsMojoValid()) {
    // Gracefully fail if we find an invalid state in the history.
    std::move(callback).Run(nullptr);
    return;
  }
  history_current_index_--;
  // Only increase undo count if there are states to undo.
  session_events_.undo_count += 1;

  active_mojo_state_ = previous_state->mojo_state()->Clone();

  std::move(callback).Run(active_mojo_state_->Clone());
}

void ComposeSession::Redo(RedoCallback callback) {
  // Redo should only be called if a forward saved state exists.
  if (history_current_index_ >= history_.size() - 1) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto next_state = CurrentState(1);
  if (!next_state->IsMojoValid()) {
    // Gracefully fail if we find an invalid state in the history.
    std::move(callback).Run(nullptr);
    return;
  }
  history_current_index_++;
  // Only increase redo count if there are states to redo.
  session_events_.redo_count += 1;

  active_mojo_state_ = next_state->mojo_state()->Clone();

  std::move(callback).Run(active_mojo_state_->Clone());
}

void ComposeSession::OpenBugReportingLink() {
  const char* url = kComposeBugReportURL;
  if (auto last_response_state = LastResponseState();
      last_response_state.has_value()) {
    if (last_response_state->mojo_state() &&
        last_response_state->mojo_state()
            ->response->on_device_evaluation_used) {
      url = kOnDeviceComposeBugReportURL;
    }
  }
  web_contents_->OpenURL(
      content::OpenURLParams(GURL(url), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /* is_renderer_initiated= */ false),
      /*navigation_handle_callback=*/{});
}

void ComposeSession::OpenComposeLearnMorePage() {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(kComposeLearnMorePageURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          /* is_renderer_initiated= */ false),
      /*navigation_handle_callback=*/{});
}

void ComposeSession::OpenFeedbackSurveyLink() {
  const char* url = kComposeFeedbackSurveyURL;
  if (auto last_response_state = LastResponseState();
      last_response_state.has_value()) {
    if (last_response_state->mojo_state() &&
        last_response_state->mojo_state()
            ->response->on_device_evaluation_used) {
      url = kOnDeviceComposeFeedbackSurveyURL;
    }
  }
  web_contents_->OpenURL(
      content::OpenURLParams(GURL(url), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /* is_renderer_initiated= */ false),
      /*navigation_handle_callback=*/{});
}

void ComposeSession::OpenSignInPage() {
  web_contents_->OpenURL(
      content::OpenURLParams(GURL(kSignInPageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /* is_renderer_initiated= */ false),
      /*navigation_handle_callback=*/{});
}

bool ComposeSession::CanShowFeedbackPage() {
  if (skip_feedback_ui_for_testing_) {
    return false;
  }

  OptimizationGuideKeyedService* opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!opt_guide_keyed_service ||
      !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForFeedback(
          optimization_guide::proto::LogAiDataRequest::FeatureCase::kCompose)) {
    return false;
  }

  return true;
}

void ComposeSession::OpenFeedbackPage(std::string feedback_id) {
  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", feedback_id);

  chrome::ShowFeedbackPage(
      web_contents_->GetLastCommittedURL(),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_COMPOSE_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"compose",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void ComposeSession::SetUserFeedback(compose::mojom::UserFeedback feedback) {
  auto last_response_state = LastResponseState();
  if (!last_response_state.has_value() || !last_response_state->mojo_state()) {
    // If there is no recent State there is nothing that we should be applying
    // feedback to.
    return;
  }

  // Save feedback to the last state associated with a valid response.
  last_response_state->mojo_state()->feedback = feedback;
  // Update `active_mojo_state_`, as it is returned by RequestInitialState()
  // when resuming a saved session.
  if (active_mojo_state_->response) {
    active_mojo_state_->feedback = feedback;
  }
  optimization_guide::proto::UserFeedback user_feedback =
      OptimizationFeedbackFromComposeFeedback(feedback);

  // Apply feedback to the last saved state with a valid response.
  optimization_guide::proto::ComposeQuality* quality =
      last_response_state->modeling_log_entry()
          ->quality_data<optimization_guide::ComposeFeatureTypeMap>();
  if (quality) {
    quality->set_user_feedback(user_feedback);
  }
  if (feedback == compose::mojom::UserFeedback::kUserFeedbackNegative) {
    session_events_.has_thumbs_down = true;
    if (CanShowFeedbackPage()) {
      // Open the Feedback Page for a thumbs down using current request log.
      std::string feedback_id = last_response_state->modeling_log_entry()
                                    ->log_ai_data_request()
                                    ->model_execution_info()
                                    .execution_id();
      OpenFeedbackPage(feedback_id);
    }
  } else if (feedback == compose::mojom::UserFeedback::kUserFeedbackPositive) {
    session_events_.has_thumbs_up = true;
  }
}

void ComposeSession::EditResult(const std::string& new_result,
                                EditResultCallback callback) {
  // If there is no change in result text resulting from the edit, do nothing.
  if (new_result == CurrentState()->mojo_state()->response->result) {
    std::move(callback).Run(false);
    return;
  }

  // Update the active state to reflect a new edit.
  active_mojo_state_->response->result = new_result;
  active_mojo_state_->response->undo_available = true;
  active_mojo_state_->response->redo_available = false;
  active_mojo_state_->response->provided_by_user = true;
  active_mojo_state_->feedback =
      compose::mojom::UserFeedback::kUserFeedbackUnspecified;

  if (CurrentState()->is_user_edited()) {
    // The current state being edited is an edit itself. In this case, update
    // its result text instead of saving a new state.
    EraseForwardStatesInHistory();
    CurrentState()->mojo_state()->response->result = new_result;
  } else {
    // The current state being edited is a server response - save the edit as a
    // new ComposeState.
    CurrentState()->mojo_state()->response->redo_available = true;

    // Add a new ComposeState to `history_` to represent the new result edit.
    auto new_state =
        std::make_unique<ComposeState>(nullptr, active_mojo_state_.Clone());
    new_state->SetUserEdited(CurrentState()->mojo_state()->response->result);

    AddNewResponseToHistory(std::move(new_state));
  }
  std::move(callback).Run(true);
  session_events_.result_edit_count += 1;
}

void ComposeSession::InitializeWithText(std::string_view selected_text) {
  // In some cases (FRE not shown, MSBB not accepted), we wait to extract the
  // inner text until all conditions are met to enable the feature.  However, if
  // we want to extract the inner text content later, we still need to store the
  // selected text.
  initial_input_ = std::string(selected_text);
  session_events_.has_initial_text = !selected_text.empty();

  MaybeRefreshPageContext(!initial_input_.empty());
}

void ComposeSession::MaybeRefreshPageContext(bool has_selection) {
  // Update dialog state based on the current selection which can change while
  // the dialog is hidden.
  currently_has_selection_ = has_selection;

  ++session_events_.compose_dialog_open_count;

  if (!fre_complete_) {
    ++session_events_.fre_view_count;
    return;
  }
  if (!current_msbb_state_) {
    ++session_events_.msbb_view_count;
    return;
  }

  // Session is initialized at the main dialog UI state.
  ++session_events_.compose_prompt_view_count;

  RefreshInnerText();
  RefreshAXSnapshot();

  // We should only autocompose once per session
  if (has_checked_autocompose_) {
    return;
  }

  // Autocompose if it is enabled and there is a valid selection.
  if (compose::GetComposeConfig().auto_submit_with_selection &&
      IsValidComposePrompt(initial_input_)) {
    Compose(initial_input_, compose::mojom::InputMode::kUnset, false);
  }
  has_checked_autocompose_ = true;
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
  std::string trimmed_inner_text;
  std::optional<uint64_t> node_offset;
  if (result) {
    const compose::Config& config = compose::GetComposeConfig();
    inner_text = std::move(result->inner_text);
    node_offset = result->node_offset;
    if (node_offset.has_value()) {
      trimmed_inner_text = compose::GetTrimmedPageText(
          inner_text, config.trimmed_inner_text_max_chars, node_offset.value(),
          config.trimmed_inner_text_header_length);
    } else {
      trimmed_inner_text =
          inner_text.substr(0, config.trimmed_inner_text_max_chars);
    }
    compose::LogComposeDialogInnerTextSize(inner_text.size());
    if (inner_text.size() > config.inner_text_max_bytes) {
      compose::LogComposeDialogInnerTextShortenedBy(
          inner_text.size() - config.inner_text_max_bytes);
      inner_text.erase(config.inner_text_max_bytes);
    }
    compose::LogComposeDialogInnerTextOffsetFound(node_offset.has_value());
  }

  if (!session_) {
    return;
  }

  if (!page_metadata_) {
    page_metadata_.emplace();
  }

  if (node_offset.has_value()) {
    page_metadata_->set_page_inner_text_offset(node_offset.value());
  }
  page_metadata_->set_trimmed_page_inner_text(trimmed_inner_text);

  page_metadata_->set_page_inner_text(std::move(inner_text));

  TryContinueComposeWithContext();
}

void ComposeSession::UpdateAXSnapshotAndContinueComposeIfNecessary(
    int request_id,
    ui::AXTreeUpdate& update) {
  if (current_ax_snapshot_request_id_ != request_id) {
    return;
  }

  got_ax_snapshot_ = true;
  if (!page_metadata_) {
    page_metadata_.emplace();
  }

  optimization_guide::PopulateAXTreeUpdateProto(
      update, page_metadata_->mutable_ax_tree_update());

  TryContinueComposeWithContext();
}

void ComposeSession::TryContinueComposeWithContext() {
  if (!HasNecessaryPageContext() || continue_compose_.is_null()) {
    return;
  }

  if (!collect_inner_text_ && !collect_ax_snapshot_) {
    // Make sure we populate the url and title even if we're not collecting
    // other context information.
    page_metadata_.emplace();
  }

  optimization_guide::proto::ComposeRequest request;
  if (page_metadata_) {
    page_metadata_->set_page_url(web_contents_->GetLastCommittedURL().spec());
    page_metadata_->set_page_title(
        base::UTF16ToUTF8(web_contents_->GetTitle()));

    *request.mutable_page_metadata() = std::move(*page_metadata_);
    page_metadata_.reset();

    session_->AddContext(request);
  }

  std::move(continue_compose_).Run();
}

void ComposeSession::RefreshInnerText() {
  got_inner_text_ = false;
  if (!collect_inner_text_) {
    return;
  }

  ++current_inner_text_request_id_;

  inner_text_caller_->GetInnerText(
      *web_contents_->GetPrimaryMainFrame(),
      // This unsafeValue call is acceptable here because node_id is a
      // FieldRendererId which while being an U64 type is based one the int
      // DOMid which we are querying here.
      node_id_.renderer_id.GetUnsafeValue(),
      base::BindOnce(
          &ComposeSession::UpdateInnerTextAndContinueComposeIfNecessary,
          weak_ptr_factory_.GetWeakPtr(), current_inner_text_request_id_));
}

void ComposeSession::RefreshAXSnapshot() {
  got_ax_snapshot_ = false;
  if (!collect_ax_snapshot_) {
    return;
  }

  ++current_ax_snapshot_request_id_;

  web_contents_->RequestAXTreeSnapshot(
      base::BindOnce(
          &ComposeSession::UpdateAXSnapshotAndContinueComposeIfNecessary,
          weak_ptr_factory_.GetWeakPtr(), current_ax_snapshot_request_id_),
      ui::kAXModeWebContentsOnly,
      compose::GetComposeConfig().max_ax_node_count_for_page_context,
      /*timeout=*/{},
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

void ComposeSession::SetFirstRunCloseReason(
    compose::ComposeFreOrMsbbSessionCloseReason close_reason) {
  fre_close_reason_ = close_reason;

  if (close_reason == compose::ComposeFreOrMsbbSessionCloseReason::
                          kAckedOrAcceptedWithoutInsert) {
    if (current_msbb_state_) {
      // The FRE dialog progresses directly to the main dialog.
      session_events_.compose_prompt_view_count = 1;
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
  MaybeRefreshPageContext(currently_has_selection_);
}

void ComposeSession::SetMSBBCloseReason(
    compose::ComposeFreOrMsbbSessionCloseReason close_reason) {
  msbb_close_reason_ = close_reason;
}

void ComposeSession::SetCloseReason(
    compose::ComposeSessionCloseReason close_reason) {
  if (close_reason == compose::ComposeSessionCloseReason::kCloseButtonPressed &&
      active_mojo_state_->has_pending_request) {
    close_reason_ =
        compose::ComposeSessionCloseReason::kCanceledBeforeResponseReceived;
  } else {
    close_reason_ = close_reason;
  }

  switch (close_reason) {
    case compose::ComposeSessionCloseReason::kCloseButtonPressed:
    case compose::ComposeSessionCloseReason::kCanceledBeforeResponseReceived:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_ABANDONED;
      final_model_status_ = optimization_guide::proto::FinalModelStatus::
          FINAL_MODEL_STATUS_FAILURE;
      session_events_.close_clicked = true;
      break;
    case compose::ComposeSessionCloseReason::kReplacedWithNewSession:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_ABANDONED;
      final_model_status_ = optimization_guide::proto::FinalModelStatus::
          FINAL_MODEL_STATUS_FAILURE;
      break;
    case compose::ComposeSessionCloseReason::kExceededMaxDuration:
    case compose::ComposeSessionCloseReason::kAbandoned:
      final_status_ = optimization_guide::proto::FinalStatus::
          STATUS_FINISHED_WITHOUT_INSERT;
      final_model_status_ = optimization_guide::proto::FinalModelStatus::
          FINAL_MODEL_STATUS_FAILURE;
      break;
    case compose::ComposeSessionCloseReason::kInsertedResponse:
      final_status_ = optimization_guide::proto::FinalStatus::STATUS_INSERTED;
      final_model_status_ = optimization_guide::proto::FinalModelStatus::
          FINAL_MODEL_STATUS_SUCCESS;
      session_events_.inserted_results = true;
      if (CurrentState().has_value() && CurrentState()->is_user_edited()) {
        session_events_.edited_result_inserted = true;
      }
      break;
    case compose::ComposeSessionCloseReason::kEndedAtFre:
    case compose::ComposeSessionCloseReason::kAckedFreEndedAtMsbb:
    case compose::ComposeSessionCloseReason::kEndedAtMsbb:
      // If the session ended during the FRE no need to set |final_status_|
      break;
  }
}

bool ComposeSession::HasExpired() {
  return session_duration_->Elapsed() >
         compose::GetComposeConfig().session_max_allowed_lifetime;
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
    if (most_recent_error_log_) {
      optimization_guide::ModelQualityLogEntry::Upload(
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
    SetMSBBCloseReason(compose::ComposeFreOrMsbbSessionCloseReason::
                           kAckedOrAcceptedWithoutInsert);
    base::RecordAction(
        base::UserMetricsAction("Compose.DialogSeen.MainDialog"));

    // Reset this initial state so that this block is not re-executed on every
    // subsequent dialog open.
    msbb_initially_off_ = false;
  }
}

void ComposeSession::SetSkipFeedbackUiForTesting(bool allowed) {
  skip_feedback_ui_for_testing_ = allowed;
}

void ComposeSession::LaunchHatsSurvey(
    compose::ComposeSessionCloseReason close_reason) {
  std::string trigger;
  switch (close_reason) {
    case compose::ComposeSessionCloseReason::kCloseButtonPressed:
      if (!base::FeatureList::IsEnabled(
              compose::features::kHappinessTrackingSurveysForComposeClose)) {
        return;
      }
      trigger = kHatsSurveyTriggerComposeClose;
      break;
    case compose::ComposeSessionCloseReason::kInsertedResponse:
      if (!base::FeatureList::IsEnabled(
              compose::features::
                  kHappinessTrackingSurveysForComposeAcceptance)) {
        return;
      }
      trigger = kHatsSurveyTriggerComposeAcceptance;

      break;
    default:
      return;
  }

  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  // Determine if the user used any of the response modifiers.
  bool response_modified =
      session_events_.shorten_count > 0 || session_events_.lengthen_count > 0 ||
      session_events_.formal_count > 0 || session_events_.casual_count > 0;

  SurveyBitsData product_specific_bits_data = {
      {compose::hats::HatsFields::kResponseModified, response_modified},
      {compose::hats::HatsFields::kSessionContainedFilteredResponse,
       session_events_.session_contained_filtered_response},
      {compose::hats::HatsFields::kSessionContainedError,
       session_events_.session_contained_any_error},
      {compose::hats::HatsFields::kSessionBeganWithNudge,
       session_events_.started_with_proactive_nudge}};

  std::string url = web_contents_->GetLastCommittedURL().spec();
  std::string session_id = session_id_.ToString();

  SurveyStringData product_specific_string_data = {
      {compose::hats::HatsFields::kSessionID, session_id},
      {compose::hats::HatsFields::kURL, url},
      {compose::hats::HatsFields::kLocale,
       g_browser_process->GetApplicationLocale()}};

  hats_service->LaunchSurveyForWebContents(trigger, web_contents_,
                                           product_specific_bits_data,
                                           product_specific_string_data);
}
