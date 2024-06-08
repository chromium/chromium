// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
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
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "content/public/browser/render_frame_host.h"
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

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(AIManagerImpl);

AIManagerImpl::AIManagerImpl(content::RenderFrameHost* rfh)
    : DocumentUserData<AIManagerImpl>(rfh) {
  browser_context_ = rfh->GetBrowserContext()->GetWeakPtr();
}

AIManagerImpl::~AIManagerImpl() = default;

// static
void AIManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  AIManagerImpl* model_manager =
      AIManagerImpl::GetOrCreateForCurrentDocument(render_frame_host);
  model_manager->receiver_.Bind(std::move(receiver));
}

void AIManagerImpl::CanCreateTextSession(
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
        base::BindOnce(&AIManagerImpl::OnModelPathValidationComplete,
                       weak_factory_.GetWeakPtr(), model_path.value()));
  }
  // If the model path is not provided, we skip the model path check.
  CanOptimizationGuideKeyedServiceCreateGenericSession(std::move(callback));
}

void AIManagerImpl::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    CreateTextSessionCallback callback) {
  content::BrowserContext* browser_context = browser_context_.get();
  if (!browser_context) {
    receiver_.ReportBadMessage(
        "Caller should ensure `CanStartModelExecutionSession()` "
        "returns true before calling this method.");
    std::move(callback).Run(/*success=*/false);
    return;
  }

  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!service) {
    receiver_.ReportBadMessage(
        "Caller should ensure `CanStartModelExecutionSession()` "
        "returns true before calling this method.");
    std::move(callback).Run(/*success=*/false);
    return;
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
  // TODO(leimy): after this check is done by optimization guide and we can
  // return that from `CanStartModelExecutionSession()`, we should replace this
  // block by a CHECK, and stop returning any boolean value from this method.
  if (!session) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  // The new `AITextSession` shares the same lifetime with the
  // `receiver`.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AITextSession>(std::move(session)), std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void AIManagerImpl::GetDefaultTextSessionSamplingParams(
    GetDefaultTextSessionSamplingParamsCallback callback) {
  std::move(callback).Run(blink::mojom::AITextSessionSamplingParams::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
}

std::string ConvertOnDeviceModelEligibilityReasonToString(
    optimization_guide::OnDeviceModelEligibilityReason debug_reason) {
  switch (debug_reason) {
    case optimization_guide::OnDeviceModelEligibilityReason::kUnknown:
      return "The service is unable to create new session.";
    case optimization_guide::OnDeviceModelEligibilityReason::kFeatureNotEnabled:
      return "The feature flag gating model execution was disabled.";
    case optimization_guide::OnDeviceModelEligibilityReason::kModelNotAvailable:
      return "There was no model available.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kConfigNotAvailableForFeature:
      return "The model was available but there was not an execution config "
             "available for the feature.";
    case optimization_guide::OnDeviceModelEligibilityReason::kGpuBlocked:
      return "The GPU is blocked.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentCrashes:
      return "The model process crashed too many times for this version.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentTimeouts:
      return "The model took too long too many times for this version.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyModelNotAvailable:
      return "The safety model was required but not available.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyConfigNotAvailableForFeature:
      return "The safety model was available but there was not a safety config "
             "available for the feature.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kLanguageDetectionModelNotAvailable:
      return "The language detection model was required but not available.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kFeatureExecutionNotEnabled:
      return "Model execution for this feature was not enabled.";
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelAdaptationNotAvailable:
      return "Model adaptation was required but not available.";
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationPending:
      return "Model validation is still pending.";
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationFailed:
      return "Model validation failed.";
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED_IN_MIGRATION();
  }
  return "";
}

void AIManagerImpl::CanOptimizationGuideKeyedServiceCreateGenericSession(
    CanCreateTextSessionCallback callback) {
  content::BrowserContext* browser_context = browser_context_.get();
  CHECK(browser_context);
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  // If the `OptimizationGuideKeyedService` cannot be retrieved, return false.
  if (!service) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "Unable to create a text session because the service is not running.");
    std::move(callback).Run(/*can_create=*/false);
    return;
  }

  // If the `OptimizationGuideKeyedService` cannot create new session, return
  // false.
  optimization_guide::OnDeviceModelEligibilityReason debug_reason;
  if (!service->CanCreateOnDeviceSession(
          optimization_guide::ModelBasedCapabilityKey::kPromptApi,
          &debug_reason)) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        ConvertOnDeviceModelEligibilityReasonToString(debug_reason));
    std::move(callback).Run(/*can_create=*/false);
    return;
  }

  std::move(callback).Run(/*can_create=*/true);
}

void AIManagerImpl::OnModelPathValidationComplete(const std::string& model_path,
                                                  bool is_valid_path) {
  if (!is_valid_path) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf("Unable to create a text session because the model "
                           "path ('%s') is invalid.",
                           model_path.c_str()));
  }
}
