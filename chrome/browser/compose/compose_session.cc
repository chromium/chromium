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
#include "components/compose/proto/compose_metadata.pb.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"

ComposeSession::ComposeSession(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideModelExecutor* executor)
    : executor_(executor),
      handler_receiver_(this),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  state_ = compose::mojom::ComposeState::New();
  state_->style = compose::mojom::StyleModifiers::New();
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
  SaveNewComposeRequest(std::move(style));
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!executor_ ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    ProcessError(l10n_util::GetStringUTF8(IDS_COMPOSE_CONFIGURATION_ERROR));
    return;
  }

  compose_proto::ComposePageMetadata page_metadata;
  page_metadata.set_page_url(web_contents_->GetLastCommittedURL().spec());
  page_metadata.set_page_title(base::UTF16ToUTF8(web_contents_->GetTitle()));

  compose_proto::ComposeRequest request;
  request.set_user_input(input);
  request.set_tone(ComposeTone(state_->style->tone));
  request.set_length(ComposeLength(state_->style->length));
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
  state_->has_pending_request = false;

  if (!result.has_value()) {
    // TODO(b/302748001 Add proper error handler.
    ProcessError("");
    return;
  }

  auto response =
      optimization_guide::ParsedAnyMetadata<compose_proto::ComposeResponse>(
          result.value());

  if (!response) {
    ProcessError("");
    return;
  }

  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();

  state_->response = ui_response->Clone();

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(std::move(ui_response));
  }
}

void ComposeSession::ProcessError(const std::string& message) {
  state_->has_pending_request = false;
  state_->response = compose::mojom::ComposeResponse::New();
  state_->response->status = compose::mojom::ComposeStatus::kError;

  if (dialog_remote_.is_bound()) {
    dialog_remote_->ResponseReceived(state_->response->Clone());
  }
}

void ComposeSession::SaveNewComposeRequest(
    compose::mojom::StyleModifiersPtr style) {
  state_ = compose::mojom::ComposeState::New();
  state_->has_pending_request = true;
  state_->style = std::move(style);
}

void ComposeSession::RequestInitialState(RequestInitialStateCallback callback) {
  std::move(callback).Run(compose::mojom::OpenMetadata::New(state_->Clone()));
}

void ComposeSession::SaveWebUIState(const std::string& webui_state) {
  state_->webui_state = webui_state;
}
