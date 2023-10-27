// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_

#include <memory>
#include <stack>
#include <string>

#include "base/check_op.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

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
class ComposeSession : public compose::mojom::ComposeDialogPageHandler {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  ComposeSession(content::WebContents* web_contents,
                 optimization_guide::OptimizationGuideModelExecutor* executor,
                 ComposeCallback callback = base::NullCallback());
  ~ComposeSession() override;

  // Binds this to a Compose webui.
  void Bind(
      mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
      mojo::PendingRemote<compose::mojom::ComposeDialog> dialog);

  // ComposeDialogPageHandler

  // Requests a compose response for `input`. The result will be sent through
  // the ComposeDialog interface rather than through a callback, as it might
  // complete after the originating WebUI has been destroyed.
  void Compose(compose::mojom::StyleModifiersPtr style,
               const std::string& input) override;

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

  // Non-ComposeDialogPageHandler Methods

  // Saves the last OK response state to the undo stack.
  void SaveLastOKStateToUndoStack();

  void set_compose_callback(ComposeCallback callback) {
    callback_ = std::move(callback);
  }

  // Sets an initial input value for the session given by the renderer.
  void set_initial_input(const std::string input) { initial_input_ = input; }

 private:
  void ProcessError(compose::mojom::ComposeStatus status);
  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelExecutionResult result);

  // Outlives `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> executor_;

  mojo::Receiver<compose::mojom::ComposeDialogPageHandler> handler_receiver_;
  mojo::Remote<compose::mojom::ComposeDialog> dialog_remote_;

  // Initialized during construction, and always remains valid during the
  // lifetime of ComposeSession.
  compose::mojom::ComposeStatePtr current_state_;

  // The last state that received a kOk status and valid response text.
  compose::mojom::ComposeStatePtr last_ok_state_;

  // The state returned when user clicks undo.
  std::stack<compose::mojom::ComposeStatePtr> undo_states_;

  // Renderer provided text selection.
  std::string initial_input_;

  // ComposeSession is owned by WebContentsUserData, so `web_contents_` outlives
  // `this`.
  raw_ptr<content::WebContents> web_contents_;

  // A callback to Autofill that triggers filling the field.
  ComposeCallback callback_;

  base::WeakPtrFactory<ComposeSession> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_SESSION_H_
