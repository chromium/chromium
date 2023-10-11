// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/proto/compose.pb.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

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
  // TODO(b/300974056): Move this to the overall feature-enabled check.
  if (!base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError, ""));
    return;
  }
  auto* model_executor = GetModelExecutor();
  DCHECK(model_executor) << "Unable to acquire model executor.";
  compose_proto::ComposeRequest request;
  request.set_input(input);
  model_executor->ExecuteModel(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_1,
      request,
      base::BindOnce(&ChromeComposeClient::ModelExecutionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeComposeClient::ModelExecutionCallback(
    ComposeCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  // TODO(b/302748001 Add proper error handler.
  if (!result.has_value()) {
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError, ""));
    return;
  }
  auto response =
      optimization_guide::ParsedAnyMetadata<compose_proto::ComposeResponse>(
          result.value());

  if (!response) {
    std::move(callback).Run(compose::mojom::ComposeResponse::New(
        compose::mojom::ComposeStatus::kError, ""));
    return;
  }

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
  last_compose_field_id_ = trigger_field.global_id();
  // TODO(b/301609035) Add the compose dialog call here.
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
}

optimization_guide::OptimizationGuideModelExecutor*
ChromeComposeClient::GetModelExecutor() {
  if (model_executor_for_test_) {
    return model_executor_for_test_;
  }

  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
}

void ChromeComposeClient::SetModelExecutorForTest(
    optimization_guide::OptimizationGuideModelExecutor* model_executor) {
  CHECK(model_executor);
  model_executor_for_test_ = model_executor;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
