// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/compose/type_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
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

const char kComposeBugReportURL[] = "https://goto.google.com/ccbrfd";

}  // namespace

ComposeSession::ComposeSession(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideModelExecutor* executor,
    ComposeCallback callback)
    : executor_(executor),
      handler_receiver_(this),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  callback_ = std::move(callback);
  current_state_ = compose::mojom::ComposeState::New();
  current_state_->style = compose::mojom::StyleModifiers::New();
}

ComposeSession::~ComposeSession() = default;

void ComposeSession::Bind(
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  handler_receiver_.reset();
  handler_receiver_.Bind(std::move(handler));

  dialog_remote_.reset();
  dialog_remote_.Bind(std::move(dialog));
}

// ComposeDialogPageHandler
void ComposeSession::Compose(compose::mojom::StyleModifiersPtr style,
                             const std::string& input) {
  current_state_->has_pending_request = true;
  current_state_->style = std::move(style);
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!executor_ ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    ProcessError(compose::mojom::ComposeStatus::kMisconfiguration);
    return;
  }

  optimization_guide::proto::ComposePageMetadata page_metadata;
  page_metadata.set_page_url(web_contents_->GetLastCommittedURL().spec());
  page_metadata.set_page_title(base::UTF16ToUTF8(web_contents_->GetTitle()));

  optimization_guide::proto::ComposeRequest request;
  request.set_user_input(input);
  request.set_tone(
      optimization_guide::proto::ComposeTone(current_state_->style->tone));
  request.set_length(
      optimization_guide::proto::ComposeLength(current_state_->style->length));
  *request.mutable_page_metadata() = std::move(page_metadata);
  executor_->ExecuteModel(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_COMPOSE,
      request,
      base::BindOnce(&ComposeSession::ModelExecutionCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ComposeSession::ModelExecutionCallback(
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  current_state_->has_pending_request = false;

  compose::mojom::ComposeStatus status =
      ComposeStatusFromOptimizationGuideResult(result);

  if (status != compose::mojom::ComposeStatus::kOk) {
    ProcessError(status);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result.value());

  if (!response) {
    ProcessError(compose::mojom::ComposeStatus::kTryAgain);
    return;
  }

  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();
  current_state_->response = ui_response->Clone();
  SaveLastOKStateToUndoStack();
  ui_response->undo_available = !undo_states_.empty();
  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(std::move(ui_response));
  }
}

void ComposeSession::ProcessError(compose::mojom::ComposeStatus error) {
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
  std::move(callback).Run(compose::mojom::OpenMetadata::New(
      initial_input_, current_state_->Clone()));
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
  compose::mojom::ComposeStatePtr undo_state = std::move(undo_states_.top());
  undo_states_.pop();
  if (!undo_state->response ||
      undo_state->response->status != compose::mojom::ComposeStatus::kOk ||
      undo_state->response->result == "") {
    // Gracefully fail if we find an invalid state on the undo stack.
    std::move(callback).Run(nullptr);
    return;
  }
  // State returns to the last undo_state.
  current_state_ = undo_state->Clone();
  last_ok_state_ = undo_state->Clone();
  undo_state->response->undo_available = !undo_states_.empty();
  std::move(callback).Run(std::move(undo_state));
}

void ComposeSession::OpenBugReportingLink() {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(kComposeBugReportURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated= */ false));
}

void ComposeSession::SaveLastOKStateToUndoStack() {
  if (!current_state_->response ||
      current_state_->response->status != compose::mojom::ComposeStatus::kOk ||
      current_state_->response->result == "") {
    // Attempting to save a state with an invalid response onto the undo stack.
    return;
  }
  if (last_ok_state_) {
    undo_states_.push(std::move(last_ok_state_));
  }
  last_ok_state_ = current_state_->Clone();
}
