// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_

#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

namespace ui {
struct AXTreeUpdate;
}

// A simple interface to reroute inner text calls to allow for test mocks.
class InnerTextProvider {
 public:
  virtual void GetInnerText(content::RenderFrameHost& host,
                            std::optional<int> node_id,
                            content_extraction::InnerTextCallback callback) = 0;

 protected:
  virtual ~InnerTextProvider() = default;
};

// The state of a compose session. This currently includes the model quality log
// entry, and the mojo based compose state.
class ComposeState;

// A class for managing a Compose Session. This session begins when a Compose
// Dialog is opened for a given field in a WebContents, and ends when one of the
// following occurs:
//  - Web Contents is destroyed
//  - Navigation happens
//  - User clicks "insert" on a compose response
//  - User clicks the close button in the WebUI.
//
//  This class can outlive its bound WebUI, as they come and go when the dialog
//  is shown and hidden.  It does not actively unbind its mojo connection, as
//  the Remote for a closed WebUI will just drop any incoming events.
//
//  This should be owned (indirectly) by the WebContents passed into its
//  constructor, and the `executor` MUST outlive that WebContents.
class ComposeSession
    : public compose::mojom::ComposeSessionUntrustedPageHandler {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  class Observer {
   public:
    virtual void OnSessionComplete(
        autofill::FieldGlobalId node_id,
        compose::ComposeSessionCloseReason close_reason,
        const compose::ComposeSessionEvents& events) = 0;
  };
  ComposeSession(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideModelExecutor* executor,
      base::Token session_id,
      InnerTextProvider* inner_text,
      autofill::FieldGlobalId node_id,
      bool is_page_language_supported,
      Observer* observer,
      ComposeCallback callback = base::NullCallback());
  ~ComposeSession() override;

  // Binds this to a Compose webui.
  void Bind(mojo::PendingReceiver<
                compose::mojom::ComposeSessionUntrustedPageHandler> handler,
            mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog);

  // ComposeSessionPageHandler
  // Tracks that there was a user action to cancel an input edit in the current
  // session in `session_events`.
  void LogCancelEdit() override;

  // Requests a compose response for `input`. The result will be sent through
  // the ComposeDialog interface rather than through a callback, as it might
  // complete after the originating WebUI has been destroyed.
  void Compose(const std::string& input,
               compose::mojom::InputMode mode,
               bool is_input_edited) override;

  // Requests a rewrite the last response. `style` specifies how the response
  // should be changed. An empty `style` without a tone or length requests a
  // rewrite without changes to the tone or length.
  void Rewrite(compose::mojom::StyleModifier style) override;

  // Tracks that there was a user action to edit the input in the current
  // session in `session_events`.
  void LogEditInput() override;

  // Retrieves and returns (through `callback`) state information for the last
  // field the user selected compose on.
  void RequestInitialState(RequestInitialStateCallback callback) override;

  // Saves an opaque state string for later use by the WebUI. Not written to
  // disk or processed by the Browser Process at all.
  void SaveWebUIState(const std::string& webui_state) override;

  // Revert from a server error to the last state with a kOk status and valid
  // response text.
  void RecoverFromErrorState(RecoverFromErrorStateCallback callback) override;

  // Undo to the previous saved state in the history.
  void Undo(UndoCallback callback) override;

  // Redo to the next saved state in the history.
  void Redo(RedoCallback callback) override;

  // Indicates that the compose result should be accepted by Autofill.
  // Callback<bool> indicates if the accept was successful.
  void AcceptComposeResult(
      AcceptComposeResultCallback success_callback) override;

  // Opens the Compose bug reporting page in a new tab when the dialog Thumbs
  // Down button is clicked. This implementation is designed for Fishfood only.
  void OpenBugReportingLink() override;

  // Opens the Compose Learn More page in a new tab when the "Learn more" link
  // is clicked in the FRE or Compose dialog.
  void OpenComposeLearnMorePage() override;

  // Opens the Compose feedback survey page in a new tab. This implementation is
  // designed for Dogfood only.
  void OpenFeedbackSurveyLink() override;

  // Opens the sign in page in a new tab when the "Sign in" link is clicked.
  void OpenSignInPage() override;

  // Saves the user feedback supplied form the UI to include in quality logs.
  void SetUserFeedback(compose::mojom::UserFeedback feedback) override;

  // Edits the result from the model. Callback returns true if the edit text
  // `new_result` is different from the result text.
  void EditResult(const std::string& new_result,
                  EditResultCallback callback) override;

  // Non-ComposeSessionUntrustedPageHandler Methods

  // Notifies the session that a new dialog is opening and starts. Saves the
  // |selected_text| for use as an initial prompt and refreshes innertext.
  void InitializeWithText(std::string_view selected_text);

  // If all pre-conditions are acknowledged starts refreshing page context. If
  // autocompose is enabled and has not been tried yet this session will also
  // start a compose request.
  void MaybeRefreshPageContext(bool has_selection);

  // Returns true if the feedback page can be shown. If
  // |skip_feedback_ui_for_testing_| is true then this always returns false and
  // the optimization guide checks are not done.
  bool CanShowFeedbackPage();

  // Opens the Chrome Feedback UI for Compose. |feedback_id| is returned from
  // OptimizationGuideModel result.
  void OpenFeedbackPage(std::string feedback_id);

  // Saves the last OK response state to the undo stack.
  void SaveMostRecentOkStateToUndoStack();

  void set_compose_callback(ComposeCallback callback) {
    callback_ = std::move(callback);
  }

  void set_collect_inner_text(bool collect_inner_text) {
    collect_inner_text_ = collect_inner_text;
  }

  bool get_current_msbb_state() { return current_msbb_state_; }

  void set_current_msbb_state(bool current_msbb_state);

  void set_fre_complete(bool fre_complete) { fre_complete_ = fre_complete; }

  void set_msbb_settings_opened() {
    session_events_.msbb_settings_opened = true;
  }

  bool get_fre_complete() { return fre_complete_; }

  void set_started_with_proactive_nudge() {
    session_events_.started_with_proactive_nudge = true;
  }

  void SetFirstRunCompleted();

  void SetFirstRunCloseReason(
      compose::ComposeFreOrMsbbSessionCloseReason close_reason);

  void SetMSBBCloseReason(
      compose::ComposeFreOrMsbbSessionCloseReason close_reason);

  void SetCloseReason(compose::ComposeSessionCloseReason close_reason);

  void LaunchHatsSurvey(compose::ComposeSessionCloseReason close_reason);

  void SetSkipFeedbackUiForTesting(bool allowed);

  bool HasExpired();

 private:
  void ProcessError(compose::EvalLocation eval_location,
                    compose::mojom::ComposeStatus status,
                    compose::ComposeRequestReason request_reason);
  void ModelExecutionCallback(
      const base::ElapsedTimer& request_start,
      int request_id,
      compose::ComposeRequestReason request_reason,
      bool was_input_edited,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);
  void ModelExecutionProgress(optimization_guide::StreamingResponse result);
  void ModelExecutionComplete(
      base::TimeDelta request_delta,
      compose::ComposeRequestReason request_reason,
      bool was_input_edited,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);
  void AddNewResponseToHistory(std::unique_ptr<ComposeState> new_state);
  void EraseForwardStatesInHistory();

  // Makes compose or rewrite request.
  void MakeRequest(optimization_guide::proto::ComposeRequest request,
                   compose::ComposeRequestReason request_reason,
                   bool is_input_edited);

  // RequestWithSession can either be called synchronously or on a later event
  // loop.
  void RequestWithSession(
      const optimization_guide::proto::ComposeRequest& request,
      compose::ComposeRequestReason request_reason,
      bool is_input_edited);

  // Callback for processing a timeout error for Compose request with `id`.
  void ComposeRequestTimeout(int id);

  // This function is bound to the callback for requesting inner-text.
  // `request_id` is used to identify the request.
  void UpdateInnerTextAndContinueComposeIfNecessary(
      int request_id,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void UpdateAXSnapshotAndContinueComposeIfNecessary(int request_id,
                                                     ui::AXTreeUpdate& update);

  // Continues the compose request if all page context has been received.
  // Note that this adds necessary metadata that may have been populated from
  // innerText or AXSnapshot (or both).
  void TryContinueComposeWithContext();

  // Returns true if the necessary page context has been received.
  bool HasNecessaryPageContext() const;

  void SetQualityLogEntryUponError(
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>,
      base::TimeDelta request_time,
      bool was_input_edited);

  // TODO(crbug.com/351040914): We should refactor different context pieces into
  // a common flow.
  // Refresh the inner text on session resumption.
  void RefreshInnerText();
  // Refresh the ax tree on session resumption.
  void RefreshAXSnapshot();

  // Returns a reference to the ComposeState at `history_current_index`, or at
  // `offset` from the current index if `offset` is specified, if it exists.
  base::optional_ref<ComposeState> CurrentState(int offset = 0);

  // Returns a reference to the ComposeState with a server response at or
  // directly preceding `history_current_index`, if it exists.
  base::optional_ref<ComposeState> LastResponseState();

  // Outlives `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> executor_;

  mojo::Receiver<compose::mojom::ComposeSessionUntrustedPageHandler>
      handler_receiver_;
  mojo::Remote<compose::mojom::ComposeUntrustedDialog> dialog_remote_;

  // Initialized during construction, and always remains valid during the
  // lifetime of ComposeSession. This diverges from CurrentState()->mojo_state()
  // to handle error states and store webui state changes in the dialog. This is
  // otherwise expected to be the same as CurrentState()->mojo_state().
  compose::mojom::ComposeStatePtr active_mojo_state_;

  // the most recent log that wont be stored in the undo stack.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry>
      most_recent_error_log_;

  // Tracks the position of the current state in the history. This index is only
  // valid when `history_` is non-empty.
  size_t history_current_index_ = 0;
  // The saved states that can be navigated between through Undo and Redo.
  std::vector<std::unique_ptr<ComposeState>> history_;

  // Renderer provided text selection.
  std::string initial_input_ = "";
  // True if there was selected text when the dialog was last opened.
  bool currently_has_selection_ = false;

  // The state of the MSBB preference
  bool current_msbb_state_ = false;
  bool msbb_initially_off_ = false;

  // Reason that a compose msbb session was exited, used for metrics.
  compose::ComposeFreOrMsbbSessionCloseReason msbb_close_reason_{
      compose::ComposeFreOrMsbbSessionCloseReason::kAbandoned};
  // State tracking whether the FRE has been completed
  bool fre_complete_ = false;

  // True if we have checked if autocompose is possible this session.
  bool has_checked_autocompose_ = false;

  // Reason that a FRE session was exited, used for metrics.
  compose::ComposeFreOrMsbbSessionCloseReason fre_close_reason_{
      compose::ComposeFreOrMsbbSessionCloseReason::kAbandoned};

  // Reason that a compose session was exited, used for metrics.
  compose::ComposeSessionCloseReason close_reason_{
      compose::ComposeSessionCloseReason::kAbandoned};
  // Reason that a compose session was exited, used for quality logging.
  optimization_guide::proto::FinalStatus final_status_{
      optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED};
  // Success status of a completed compose session, used for quality logging.
  optimization_guide::proto::FinalModelStatus final_model_status_{
      optimization_guide::proto::FinalModelStatus::
          FINAL_MODEL_STATUS_UNSPECIFIED};

  // Tracks how long a session has been open.
  std::unique_ptr<base::ElapsedTimer> session_duration_;

  // Map for managing client-side request timeouts.
  base::flat_map<int, std::unique_ptr<base::OneShotTimer>> request_timeouts_;

  // ComposeSession is owned by WebContentsUserData, so `web_contents_` outlives
  // `this`.
  raw_ptr<content::WebContents> web_contents_;

  raw_ptr<Observer> observer_;

  // A callback to Autofill that triggers filling the field.
  ComposeCallback callback_;

  // A session which allows for building context and streaming output.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // This is incremented every request to avoid handling responses from previous
  // requests.
  int request_id_ = 0;

  // Increasing counter used to identify most recent request for inner-text.
  int current_inner_text_request_id_ = 0;
  // Increasing counter used to identify most recent request for ax snapshot.
  int current_ax_snapshot_request_id_ = 0;

  bool collect_inner_text_;

  bool collect_ax_snapshot_ = false;

  // This pointer is to a class that owns and creates this class, so will
  // outlive the session.
  raw_ptr<InnerTextProvider> inner_text_caller_;

  // Logging counters.
  compose::ComposeSessionEvents session_events_;

  // UKM source ID.
  ukm::SourceId ukm_source_id_;

  // If true, the inner-text was received.
  bool got_inner_text_ = false;

  // If true, the ax snapshot was received.
  bool got_ax_snapshot_ = false;

  autofill::FieldGlobalId node_id_;

  // Information about the page assessed language being supported by Compose.
  bool is_page_language_supported_;

  base::OnceClosure continue_compose_;

  base::Token session_id_;

  bool skip_feedback_ui_for_testing_ = false;

  std::optional<optimization_guide::proto::ComposePageMetadata> page_metadata_;

  base::WeakPtrFactory<ComposeSession> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
