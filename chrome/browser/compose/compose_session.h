// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_

#include <memory>
#include <stack>
#include <string>

#include "base/check_op.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/compose/inner_text_extractor.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

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
class ComposeSession : public compose::mojom::ComposeSessionPageHandler {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  ComposeSession(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideModelExecutor* executor,
      optimization_guide::ModelQualityLogsUploader* model_quality_logs_uploader,
      base::Token session_id,
      ComposeCallback callback = base::NullCallback());
  ~ComposeSession() override;

  // Binds this to a Compose webui.
  void Bind(
      mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler> handler,
      mojo::PendingRemote<compose::mojom::ComposeDialog> dialog);

  // ComposeSessionPageHandler

  // Requests a compose response for `input`. The result will be sent through
  // the ComposeDialog interface rather than through a callback, as it might
  // complete after the originating WebUI has been destroyed.
  void Compose(const std::string& input, bool is_input_edited) override;

  // Requests a rewrite the last response. `style` specifies how the response
  // should be changed. An empty `style` without a tone or length requests a
  // rewrite without changes to the tone or length.
  void Rewrite(compose::mojom::StyleModifiersPtr style) override;

  // Retrieves and returns (through `callback`) state information for the last
  // field the user selected compose on.
  void RequestInitialState(RequestInitialStateCallback callback) override;

  // Saves an opaque state string for later use by the WebUI. Not written to
  // disk or processed by the Browser Process at all.
  void SaveWebUIState(const std::string& webui_state) override;

  // Undo to the last state with an kOk status and valid response text.
  void Undo(UndoCallback callback) override;

  // Indicates that the compose result should be accepted by Autofill.
  // Callback<bool> indicates if the accept was successful.
  void AcceptComposeResult(
      AcceptComposeResultCallback success_callback) override;

  // Opens the Compose bug reporting page in a new tab when the dialog Thumbs
  // Down button is clicked. This implementation is designed for Fishfood only.
  void OpenBugReportingLink() override;

  // Opens the Compose feedback survey page in a new tab. This implementation is
  // designed for Dogfood only.
  void OpenFeedbackSurveyLink() override;

  // Opens the Compose-related Chrome settings page in a new tab when the
  // "settings" link is clicked in the consent dialog.
  void OpenComposeSettings() override;

  // Saves the user feedback supplied form the UI to include in quality logs.
  void SetUserFeedback(compose::mojom::UserFeedback feedback) override;

  // Non-ComposeSessionPageHandler Methods

  // Notifies the session that a new dialog is opening and starts refreshing
  // inner text. Calls Compose immediately if the initial input is valid.
  void InitializeWithText(const std::optional<std::string>& text);

  // Opens the Chrome Feedback UI for Compose. |feedback_id| is returned from
  // OptimizationGuideModel result.
  void OpenFeedbackPage(std::string feedback_id);

  // Saves the last OK response state to the undo stack.
  void SaveMostRecentOkStateToUndoStack();

  void set_compose_callback(ComposeCallback callback) {
    callback_ = std::move(callback);
  }

  // Sets an initial input value for the session given by the renderer.
  void set_initial_input(const std::string input) { initial_input_ = input; }

  void set_skip_inner_text(bool skip_inner_text) {
    skip_inner_text_ = skip_inner_text;
  }

  void set_initial_consent_state(compose::mojom::ConsentState consent_state) {
    initial_consent_state_ = consent_state;
  }

  // Set the first time the user progresses through the consent/disclaimer
  // dialog to the main dialog. This can only be set one way as it corresponds
  // to completion of the user's FRE.
  void set_consent_given_or_acknowledged() {
    consent_given_or_acknowledged_ = true;
  }

  // Refresh the inner text on session resumption.
  void RefreshInnerText();

  void SetCloseReason(compose::ComposeSessionCloseReason close_reason);

 private:
  void ProcessError(compose::mojom::ComposeStatus status);
  void ModelExecutionCallback(
      const base::ElapsedTimer& request_start,
      int request_id,
      bool was_input_edited,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);
  // Adds page content to the session context.
  void AddPageContentToSession(const std::string& inner_text);

  // Makes compose or rewrite request.
  void MakeRequest(optimization_guide::proto::ComposeRequest request,
                   bool is_input_edited);

  // RequestWithSession can either be called synchronously or on a later event
  // loop
  void RequestWithSession(
      const optimization_guide::proto::ComposeRequest& request,
      bool is_input_edited);

  void UpdateInnerTextAndContinueComposeIfNecessary(
      const std::string& inner_text);

  void SendQualityLogEntryUponError(
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>,
      base::TimeDelta request_time);

  // Outlives `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> executor_;

  mojo::Receiver<compose::mojom::ComposeSessionPageHandler> handler_receiver_;
  mojo::Remote<compose::mojom::ComposeDialog> dialog_remote_;

  // Initialized during construction, and always remains valid during the
  // lifetime of ComposeSession.
  compose::mojom::ComposeStatePtr current_state_;

  // The most recent state that was received via a request/response pair.
  std::unique_ptr<ComposeState> most_recent_ok_state_;

  // The state returned when user clicks undo.
  std::stack<std::unique_ptr<ComposeState>> undo_states_;

  // Renderer provided text selection.
  std::string initial_input_;

  // The state of consent-related prefs when the session is first created.
  compose::mojom::ConsentState initial_consent_state_ =
      compose::mojom::ConsentState::kUnset;
  // True if the user either gave consent or acknowledged given consent in this
  // session.
  bool consent_given_or_acknowledged_ = false;

  // Reason that a compose session was exited, used for metrics.
  compose::ComposeSessionCloseReason close_reason_;
  // Reason that a compose session was exited, used for quality logging.
  optimization_guide::proto::FinalStatus final_status_;

  // ComposeSession is owned by WebContentsUserData, so `web_contents_` outlives
  // `this`.
  raw_ptr<content::WebContents> web_contents_;

  // A callback to Autofill that triggers filling the field.
  ComposeCallback callback_;

  // A session which allows for building context and streaming output.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // This is incremented every request to avoid handling responses from previous
  // requests.
  int request_id_ = 0;

  bool skip_inner_text_ = false;

  // Logging counters.
  int compose_count_ = 0;
  int dialog_shown_count_ = 0;
  int undo_count_ = 0;

  InnerTextExtractor inner_text_extractor_;
  std::optional<std::string> inner_text_;

  base::OnceClosure continue_compose_;

  // This pointer is obtained form a BrowserContextKeyedService.
  // TODO(b/314328835) Add a BrowserContextKeyedServiceShutdownNotifierFactory
  // to nullify when keyed service is destyroyed.
  raw_ptr<optimization_guide::ModelQualityLogsUploader>
      model_quality_logs_uploader_;

  base::Token session_id_;

  base::WeakPtrFactory<ComposeSession> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
