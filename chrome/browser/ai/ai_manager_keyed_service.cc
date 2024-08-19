// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_rewriter.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "chrome/browser/ai/ai_writer.h"
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
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"
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
  NOTREACHED();
}

std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
CreateComposeSession(content::BrowserContext& browser_context) {
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(&browser_context));
  if (!service) {
    return nullptr;
  }
  optimization_guide::SessionConfigParams config_params =
      optimization_guide::SessionConfigParams{.disable_server_fallback = true};
  return service->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kCompose, config_params);
}

}  // namespace

AIManagerKeyedService::AIManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AIManagerKeyedService::~AIManagerKeyedService() {}

void AIManagerKeyedService::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver,
    AIContextBoundObjectSet::ReceiverContext context) {
  receivers_.Add(this, std::move(receiver), context);
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
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    const blink::mojom::AITextSessionSamplingParamsPtr& sampling_params,
    AIContextBoundObjectSet* context_bound_object_set,
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
      std::move(session), browser_context_->GetWeakPtr(), std::move(receiver),
      context_bound_object_set, context);
}

void AIManagerKeyedService::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    const std::optional<std::string>& system_prompt,
    CreateTextSessionCallback callback) {
  // Since this is a mojo IPC implementation, the context should be non-null;
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(receivers_.current_context());
  std::unique_ptr<AITextSession> session = CreateTextSessionInternal(
      std::move(receiver), sampling_params, context_bound_object_set);
  if (!session) {
    // TODO(crbug.com/343325183): probably we should consider returning an error
    // enum and throw a clear exception from the blink side.
    std::move(callback).Run(nullptr);
    return;
  }

  if (system_prompt.has_value()) {
    // If the system prompt is provided, we need to set the system prompt and
    // invoke the callback after it.
    session->SetSystemPrompt(system_prompt.value(), std::move(callback));
  } else {
    std::move(callback).Run(session->GetTextSessionInfo());
  }

  context_bound_object_set->AddContextBoundObject(std::move(session));
}

void AIManagerKeyedService::GetTextModelInfo(
    GetTextModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AITextModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelMaxTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
}

void AIManagerKeyedService::CreateWriter(
    const std::optional<std::string>& shared_context,
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client) {
  mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
      std::move(client));
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = CreateComposeSession(*browser_context_);
  if (!session) {
    // TODO(crbug.com/357967382): Return an error enum and throw a clear
    // exception from the blink side.
    // TODO(crbug.com/357967382): Consider retrying for
    // kConfigNotAvailableForFeature case.
    client_remote->OnResult(mojo::PendingRemote<blink::mojom::AIWriter>());
    return;
  }
  // The new `AIWriter` shares the same lifetime with the passed remote.
  // TODO(crbug.com/357967382): Move the ownership of `AIWriter` to a UserData
  // structure like `AITextSessionSet`.
  mojo::PendingRemote<blink::mojom::AIWriter> pending_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AIWriter>(std::move(session), shared_context),
      pending_remote.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(pending_remote));
}

void AIManagerKeyedService::CreateRewriter(
    const std::optional<std::string>& shared_context,
    blink::mojom::AIRewriterTone tone,
    blink::mojom::AIRewriterLength length,
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client) {
  mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
      std::move(client));
  if (tone != blink::mojom::AIRewriterTone::kAsIs &&
      length != blink::mojom::AIRewriterLength::kAsIs) {
    // TODO(crbug.com/358214322): Currently the combination of the tone and the
    // length option is not supported.
    // TODO(crbug.com/358214322): Return an error enum and throw a clear
    // exception from the blink side.
    client_remote->OnResult(mojo::PendingRemote<blink::mojom::AIRewriter>());
    return;
  }
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = CreateComposeSession(*browser_context_);
  if (!session) {
    // TODO(crbug.com/358214322): Return an error enum and throw a clear
    // exception from the blink side.
    // TODO(crbug.com/358214322): Consider retrying for
    // kConfigNotAvailableForFeature case.
    client_remote->OnResult(mojo::PendingRemote<blink::mojom::AIRewriter>());
    return;
  }
  // The new `AIRewriter` shares the same lifetime with the passed remote.
  // TODO(crbug.com/358214322): Move the ownership of `AIRewriter` to a UserData
  // structure like `AITextSessionSet`.
  mojo::PendingRemote<blink::mojom::AIRewriter> pending_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AIRewriter>(std::move(session), shared_context, tone,
                                   length),
      pending_remote.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(pending_remote));
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
        /*result=*/
        blink::mojom::ModelAvailabilityCheckResult::kNoServiceNotRunning);
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
    AIContextBoundObjectSet* context_bound_object_set,
    const AITextSession::Context& context,
    CreateTextSessionCallback callback) {
  std::unique_ptr<AITextSession> session = CreateTextSessionInternal(
      std::move(receiver), sampling_params, context_bound_object_set, context);
  if (!session) {
    std::move(callback).Run(nullptr);
    return;
  }

  blink::mojom::AITextSessionInfoPtr session_info =
      session->GetTextSessionInfo();
  AIContextBoundObjectSet::GetFromContext(receivers_.current_context())
      ->AddContextBoundObject(std::move(session));
  std::move(callback).Run(std::move(session_info));
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
