// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

// Checks if the model path configured via command line is valid.
bool IsModelPathValid(const std::string& model_path_str) {
  std::optional<base::FilePath> model_path =
      optimization_guide::StringToFilePath(model_path_str);
  if (!model_path) {
    return false;
  }
  return base::PathExists(*model_path);
}

blink::mojom::ModelAvailabilityCheckResult
ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
    optimization_guide::OnDeviceModelEligibilityReason debug_reason) {
  switch (debug_reason) {
    case optimization_guide::OnDeviceModelEligibilityReason::kUnknown:
      return blink::mojom::ModelAvailabilityCheckResult::kNoUnknown;
    case optimization_guide::OnDeviceModelEligibilityReason::kFeatureNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::kNoFeatureNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::kModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::kNoModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::kGpuBlocked:
      return blink::mojom::ModelAvailabilityCheckResult::kNoGpuBlocked;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentCrashes:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoTooManyRecentCrashes;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentTimeouts:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoTooManyRecentTimeouts;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoSafetyModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoSafetyConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kLanguageDetectionModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoLanguageDetectionModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kFeatureExecutionNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoFeatureExecutionNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelAdaptationNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoModelAdaptationNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationPending:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationPending;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationFailed:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationFailed;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelToBeInstalled:
      return blink::mojom::ModelAvailabilityCheckResult::kAfterDownload;
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_NORETURN();
}

}  // namespace

AIManagerKeyedService::AIManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AIManagerKeyedService::~AIManagerKeyedService() {}

void AIManagerKeyedService::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AIManagerKeyedService::CanCreateTextSession(
    CanCreateTextSessionCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path) {
    // If the model path is provided, we do this additional check and post a
    // warning message to dev tools if it's invalid.
    // This needs to be done in a task runner with `MayBlock` trait.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(IsModelPathValid, model_path.value()),
        base::BindOnce(&AIManagerKeyedService::OnModelPathValidationComplete,
                       weak_factory_.GetWeakPtr(), model_path.value()));
  }
  // If the model path is not provided, we skip the model path check.
  CanOptimizationGuideKeyedServiceCreateGenericSession(std::move(callback));
}

std::unique_ptr<AITextSession> AIManagerKeyedService::CreateTextSessionInternal(
    const blink::mojom::AITextSessionSamplingParamsPtr& sampling_params,
    const std::optional<const AITextSession::Context>& context) {
  CHECK(browser_context_);
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_.get()));
  if (!service) {
    return nullptr;
  }

  optimization_guide::SessionConfigParams config_params =
      optimization_guide::SessionConfigParams{.disable_server_fallback = true};
  if (sampling_params) {
    config_params.sampling_params = optimization_guide::SamplingParams{
        .top_k = sampling_params->top_k,
        .temperature = sampling_params->temperature};
  }

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = service->StartSession(
          optimization_guide::ModelBasedCapabilityKey::kPromptApi,
          config_params);
  if (!session) {
    return nullptr;
  }

  return std::make_unique<AITextSession>(
      std::move(session), config_params.sampling_params,
      browser_context_->GetWeakPtr(), context);
}

void AIManagerKeyedService::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    const std::optional<std::string>& system_prompt,
    CreateTextSessionCallback callback) {
  std::unique_ptr<AITextSession> session =
      CreateTextSessionInternal(sampling_params);
  if (!session) {
    // TODO(crbug.com/343325183): probably we should consider returning an error
    // enum and throw a clear exception from the blink side.
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/356809696): instead of using `mojo::MakeSelfOwnedReceiver`,
  // the session's lifetime should be associated with either the host of the
  // document or worker.
  if (!system_prompt.has_value()) {
    // The new `AITextSession` shares the same lifetime with the `receiver`.
    mojo::MakeSelfOwnedReceiver(std::move(session), std::move(receiver));
    std::move(callback).Run(true);
    return;
  }

  // If the system prompt is provided, we need to set the system prompt and
  // invoke the callback after it.
  static_cast<AITextSession*>(
      mojo::MakeSelfOwnedReceiver(std::move(session), std::move(receiver))
          ->impl())
      ->SetSystemPrompt(system_prompt.value(), std::move(callback));
}

void AIManagerKeyedService::GetTextModelInfo(
    GetTextModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AITextModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelMaxTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
}

void AIManagerKeyedService::
    CanOptimizationGuideKeyedServiceCreateGenericSession(
        CanCreateTextSessionCallback callback) {
  CHECK(browser_context_);
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));

  // If the `OptimizationGuideKeyedService` cannot be retrieved, return false.
  if (!service) {
    std::move(callback).Run(
        /*result=*/blink::mojom::ModelAvailabilityCheckResult::
            kNoServiceNotRunning);
    return;
  }

  // If the `OptimizationGuideKeyedService` cannot create new session, return
  // false.
  optimization_guide::OnDeviceModelEligibilityReason debug_reason;
  if (!service->CanCreateOnDeviceSession(
          optimization_guide::ModelBasedCapabilityKey::kPromptApi,
          &debug_reason)) {
    std::move(callback).Run(
        /*result=*/
        ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
            debug_reason));
    return;
  }

  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
}

void AIManagerKeyedService::CreateTextSessionForCloning(
    base::PassKey<AITextSession> pass_key,
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    const AITextSession::Context& context,
    base::OnceCallback<void(bool)> callback) {
  std::unique_ptr<AITextSession> session =
      CreateTextSessionInternal(sampling_params, context);
  if (!session) {
    std::move(callback).Run(false);
    return;
  }

  mojo::MakeSelfOwnedReceiver(std::move(session), std::move(receiver));
  std::move(callback).Run(true);
}

void AIManagerKeyedService::OnModelPathValidationComplete(
    const std::string& model_path,
    bool is_valid_path) {
  // TODO(crbug.com/346491542): Remove this when the error page is implemented.
  if (!is_valid_path) {
    VLOG(1) << base::StringPrintf(
        "Unable to create a text session because the model path ('%s') is "
        "invalid.",
        model_path.c_str());
  }
}
