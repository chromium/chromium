// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_create_on_device_session_task.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"

namespace {
// Currently, the following errors, which are used when a model may have been
// installed but not yet loaded, are treated as waitable.
static constexpr auto kWaitableReasons =
    base::MakeFixedFlatSet<optimization_guide::OnDeviceModelEligibilityReason>({
        optimization_guide::OnDeviceModelEligibilityReason::
            kConfigNotAvailableForFeature,
        optimization_guide::OnDeviceModelEligibilityReason::
            kSafetyModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::
            kLanguageDetectionModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
    });
}  // namespace

CreateOnDeviceSessionTask::CreateOnDeviceSessionTask(
    content::BrowserContext* browser_context,
    optimization_guide::ModelBasedCapabilityKey feature)
    : browser_context_(browser_context), feature_(feature) {}

CreateOnDeviceSessionTask::~CreateOnDeviceSessionTask() {
  if (observing_availability_) {
    OptimizationGuideKeyedService* service = GetOptimizationGuideService();
    if (service) {
      service->RemoveOnDeviceModelAvailabilityChangeObserver(feature_, this);
    }
  }
}

void CreateOnDeviceSessionTask::Run() {
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (!service) {
    OnFinish(nullptr);
    return;
  }

  if (auto session = StartSession()) {
    OnFinish(std::move(session));
    return;
  }
  optimization_guide::OnDeviceModelEligibilityReason reason;
  bool can_create = service->CanCreateOnDeviceSession(feature_, &reason);
  CHECK(!can_create);
  if (!kWaitableReasons.contains(reason)) {
    OnFinish(nullptr);
    return;
  }
  observing_availability_ = true;
  service->AddOnDeviceModelAvailabilityChangeObserver(feature_, this);
}

void CreateOnDeviceSessionTask::Cancel() {
  CHECK(observing_availability_);
  CHECK(deletion_callback_);
  std::move(deletion_callback_).Run();
}

void CreateOnDeviceSessionTask::SetDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
}

void CreateOnDeviceSessionTask::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  if (kWaitableReasons.contains(reason)) {
    return;
  }
  OnFinish(StartSession());
  std::move(deletion_callback_).Run();
}

std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
CreateOnDeviceSessionTask::StartSession() {
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (!service) {
    return nullptr;
  }

  using ::optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
      .logging_mode = SessionConfigParams::LoggingMode::kAlwaysDisable,
  };

  UpdateSessionConfigParams(&config_params);
  return service->StartSession(feature_, config_params);
}

OptimizationGuideKeyedService*
CreateOnDeviceSessionTask::GetOptimizationGuideService() {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

CreateAssistantOnDeviceSessionTask::CreateAssistantOnDeviceSessionTask(
    content::BrowserContext* browser_context,
    const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
    base::OnceCallback<
        void(std::unique_ptr<
             optimization_guide::OptimizationGuideModelExecutor::Session>)>
        completion_callback)
    : CreateOnDeviceSessionTask(
          browser_context,
          optimization_guide::ModelBasedCapabilityKey::kPromptApi),
      completion_callback_(std::move(completion_callback)) {
  if (sampling_params) {
    sampling_params_ = optimization_guide::SamplingParams{
        .top_k = std::min(
            sampling_params->top_k,
            uint32_t(AIManagerKeyedService::GetAssistantModelMaxTopK())),
        .temperature = sampling_params->temperature};
  } else {
    sampling_params_ = optimization_guide::SamplingParams{
        .top_k = uint32_t(
            optimization_guide::features::GetOnDeviceModelDefaultTopK()),
        .temperature = float(
            AIManagerKeyedService::GetAssistantModelDefaultTemperature())};
  }
}

CreateAssistantOnDeviceSessionTask::~CreateAssistantOnDeviceSessionTask() =
    default;

void CreateAssistantOnDeviceSessionTask::OnFinish(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session) {
  std::move(completion_callback_).Run(std::move(session));
}

void CreateAssistantOnDeviceSessionTask::UpdateSessionConfigParams(
    optimization_guide::SessionConfigParams* config_params) {
  config_params->sampling_params = sampling_params_;
}
