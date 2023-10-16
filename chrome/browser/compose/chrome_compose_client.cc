// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

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

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      manager_(this) {}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  handler_receiver_ = std::make_unique<
      mojo::Receiver<compose::mojom::ComposeDialogPageHandler>>(
      this, std::move(handler));
  dialog_remote_ =
      std::make_unique<mojo::Remote<compose::mojom::ComposeDialog>>(
          std::move(dialog));
}

void ChromeComposeClient::Compose(compose::mojom::StyleModifiersPtr style,
                                  const std::string& input,
                                  ComposeCallback callback) {
  SaveNewComposeRequest(input, std::move(style));
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  auto* model_executor = GetModelExecutor();
  if (!model_executor ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    UpdateComposeStateWithResponse(compose::mojom::ComposeStatus::kError, "");
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError,
        l10n_util::GetStringUTF8(IDS_COMPOSE_CONFIGURATION_ERROR)));
    return;
  }

  compose::mojom::StyleModifiersPtr& saved_style =
      field_states_.at(last_compose_field_id_)->style;

  compose_proto::ComposePageMetadata page_metadata;
  page_metadata.set_page_url(GetWebContents().GetLastCommittedURL().spec());
  page_metadata.set_page_title(base::UTF16ToUTF8(GetWebContents().GetTitle()));

  compose_proto::ComposeRequest request;
  request.set_user_input(input);
  request.set_tone(ComposeTone(saved_style->tone));
  request.set_length(ComposeLength(saved_style->length));
  *request.mutable_page_metadata() = std::move(page_metadata);
  model_executor->ExecuteModel(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_COMPOSE,
      request,
      base::BindOnce(&ChromeComposeClient::ModelExecutionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeComposeClient::ModelExecutionCallback(
    ComposeCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  // TODO(b/302748001 Add proper error handler.
  if (!result.has_value()) {
    UpdateComposeStateWithResponse(compose::mojom::ComposeStatus::kError, "");
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError, ""));
    return;
  }
  auto response =
      optimization_guide::ParsedAnyMetadata<compose_proto::ComposeResponse>(
          result.value());

  if (!response) {
    UpdateComposeStateWithResponse(compose::mojom::ComposeStatus::kError, "");
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError, ""));
    return;
  }

  UpdateComposeStateWithResponse(compose::mojom::ComposeStatus::kOk,
                                 response->output());
  auto ui_response = compose::mojom::ComposeResponse::New();
  ui_response->status = compose::mojom::ComposeStatus::kOk;
  ui_response->result = response->output();

  std::move(callback).Run(std::move(ui_response));
}

void ChromeComposeClient::ShowComposeDialog(
    autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeDialogCallback callback) {
  SaveFieldAndCreateComposeStateIfEmpty(trigger_field.global_id());
  if (!skip_show_dialog_for_test_) {
    const gfx::RectF element_bounds = trigger_field.bounds;
    chrome::ShowComposeDialog(GetWebContents(), element_bounds);
  }
}

void ChromeComposeClient::RequestInitialState(
    RequestInitialStateCallback callback) {
  compose::mojom::ComposeStatePtr compose_state =
      field_states_.at(last_compose_field_id_)->Clone();
  std::move(callback).Run(
      compose::mojom::OpenMetadata::New(std::move(compose_state)));
}

void ChromeComposeClient::SaveFieldAndCreateComposeStateIfEmpty(
    const autofill::FieldGlobalId& field_id) {
  last_compose_field_id_ = field_id;
  auto it = field_states_.find(last_compose_field_id_);
  if (it == field_states_.end()) {
    CreateNewCurrentComposeState();
  }
}

void ChromeComposeClient::SaveNewComposeRequest(
    const std::string& input,
    compose::mojom::StyleModifiersPtr style) {
  MaybeSaveCurrentStateInUndoStack();
  // Overwrite the existing state, after we save it for undo.
  CreateNewCurrentComposeState();

  auto& compose_state = field_states_.at(last_compose_field_id_);
  compose_state->has_pending_request = true;
  compose_state->input = input;
  compose_state->style = std::move(style);
}

void ChromeComposeClient::MaybeSaveCurrentStateInUndoStack() {
  // TODO(b/302741370) Save the existing current state in the undo stack if
  // there is a valid response. Undo states are not saved if there is no
  // response, or if the response is an error.
}

void ChromeComposeClient::CreateNewCurrentComposeState() {
  auto compose_state = compose::mojom::ComposeState::New();
  compose_state->style = compose::mojom::StyleModifiers::New();
  compose_state->style->tone = compose::mojom::Tone::kUnset;
  compose_state->style->length = compose::mojom::Length::kUnset;
  field_states_.emplace(last_compose_field_id_, std::move(compose_state));
}

void ChromeComposeClient::UpdateComposeStateWithResponse(
    compose::mojom::ComposeStatus status,
    const std::string& response_text) {
  auto& compose_state = field_states_.at(last_compose_field_id_);
  compose_state->has_pending_request = false;
  compose_state->response = compose::mojom::ComposeResponse::New();
  compose_state->response->status = status;
  compose_state->response->result = response_text;
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
}

optimization_guide::OptimizationGuideModelExecutor*
ChromeComposeClient::GetModelExecutor() {
  return model_executor_for_test_.value_or(
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())));
}

void ChromeComposeClient::SetModelExecutorForTest(
    optimization_guide::OptimizationGuideModelExecutor* model_executor) {
  model_executor_for_test_ = model_executor;
}

void ChromeComposeClient::SetSkipShowDialogForTest() {
  skip_show_dialog_for_test_ = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
