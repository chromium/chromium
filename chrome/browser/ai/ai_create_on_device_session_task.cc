// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_create_on_device_session_task.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"

namespace {

// Currently, the following errors, which are used when a model is being
// downloaded or have been installed but not yet loaded, are treated as
// waitable.
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
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (service) {
    service->RemoveOnDeviceModelAvailabilityChangeObserver(feature_, this);
  }
}

void CreateOnDeviceSessionTask::Finish(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session) {
  CHECK(state_ != State::kCancelled && state_ != State::kFinished);
  SetState(State::kFinished);
  OnFinish(std::move(session));
}

void CreateOnDeviceSessionTask::Start() {
  CHECK(state_ == State::kNotStarted);
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (!service) {
    Finish(nullptr);
    return;
  }

  if (auto session = StartSession()) {
    Finish(std::move(session));
    return;
  }
  optimization_guide::OnDeviceModelEligibilityReason reason;
  bool can_create = service->CanCreateOnDeviceSession(feature_, &reason);
  CHECK(!can_create);
  if (!kWaitableReasons.contains(reason)) {
    Finish(nullptr);
    return;
  }
  SetState(State::kPending);
  service->AddOnDeviceModelAvailabilityChangeObserver(feature_, this);
}

void CreateOnDeviceSessionTask::Cancel() {
  SetState(State::kCancelled);
  if (deletion_callback_) {
    std::move(deletion_callback_).Run();
  }
}

void CreateOnDeviceSessionTask::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  CHECK(state_ == State::kPending);
  if (kWaitableReasons.contains(reason)) {
    return;
  }
  Finish(StartSession());
  if (deletion_callback_) {
    std::move(deletion_callback_).Run();
  }
}

void CreateOnDeviceSessionTask::SetDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
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

std::ostream& operator<<(std::ostream& os,
                         CreateOnDeviceSessionTask::State state) {
  switch (state) {
    case CreateOnDeviceSessionTask::State::kCancelled:
      os << "Cancelled";
      break;
    case CreateOnDeviceSessionTask::State::kFinished:
      os << "Finished";
      break;
    case CreateOnDeviceSessionTask::State::kNotStarted:
      os << "Not Started";
      break;
    case CreateOnDeviceSessionTask::State::kPending:
      os << "Pending";
      break;
    default:
      os << "<invalid value: " << static_cast<int>(state) << ">";
  }
  return os;
}

void CreateOnDeviceSessionTask::SetState(State state) {
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kNotStarted, {State::kFinished, State::kPending}},
          {State::kPending, {State::kFinished, State::kCancelled}},
      }));

  DCHECK_STATE_TRANSITION(transitions, state_, state);
  state_ = state;
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
  AIManagerKeyedService* service =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(browser_context);
  if (sampling_params) {
    sampling_params_ = optimization_guide::SamplingParams{
        .top_k = std::min(sampling_params->top_k,
                          service->GetAssistantModelMaxTopK()),
        .temperature = sampling_params->temperature};
  } else {
    sampling_params_ = service->GetAssistantDefaultSamplingParams();
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
