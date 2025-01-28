// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_create_on_device_session_task.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/to_string.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/ai/built_in_ai_logger.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

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
    AIContextBoundObjectSet& context_bound_object_set,
    content::BrowserContext* browser_context,
    optimization_guide::ModelBasedCapabilityKey feature)
    : AIContextBoundObject(context_bound_object_set),
      browser_context_(browser_context),
      feature_(feature) {}

CreateOnDeviceSessionTask::~CreateOnDeviceSessionTask() {
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (service) {
    service->RemoveOnDeviceModelAvailabilityChangeObserver(feature_, this);
  }
}

void CreateOnDeviceSessionTask::Finish(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session) {
  SetState(State::kFinished);
  OnFinish(std::move(session));
}

void CreateOnDeviceSessionTask::Start() {
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (!service) {
    Finish(nullptr);
    return;
  }

  if (auto session = StartSession()) {
    Finish(std::move(session));
    return;
  }
  auto eligibility = service->GetOnDeviceModelEligibility(feature_);
  CHECK_NE(eligibility,
           optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  if (!kWaitableReasons.contains(eligibility)) {
    BUILT_IN_AI_LOGGER() << "Cannot create session for feature '" << feature_
                         << "'. " << "Reason: " << eligibility;
    Finish(nullptr);
    return;
  }
  SetState(State::kPending);
  service->AddOnDeviceModelAvailabilityChangeObserver(feature_, this);
}

void CreateOnDeviceSessionTask::Cancel() {
  SetState(State::kCancelled);
  RemoveFromSet();
}

void CreateOnDeviceSessionTask::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  bool waitable = kWaitableReasons.contains(reason);
  BUILT_IN_AI_LOGGER() << "Feature '" << feature << "' "
                       << "availability changed due to '" << reason << "'. "
                       << "Waitable: " << base::ToString(waitable);
  CHECK(state_ == State::kPending);
  if (waitable) {
    return;
  }
  Finish(StartSession());
  RemoveFromSet();
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

CreateLanguageModelOnDeviceSessionTask::CreateLanguageModelOnDeviceSessionTask(
    AIManager& ai_manager,
    AIContextBoundObjectSet& context_bound_object_set,
    content::BrowserContext* browser_context,
    const blink::mojom::AILanguageModelSamplingParamsPtr& sampling_params,
    base::OnceCallback<
        void(std::unique_ptr<
             optimization_guide::OptimizationGuideModelExecutor::Session>)>
        completion_callback)
    : CreateOnDeviceSessionTask(
          context_bound_object_set,
          browser_context,
          optimization_guide::ModelBasedCapabilityKey::kPromptApi),
      completion_callback_(std::move(completion_callback)) {
  auto language_model_params = ai_manager.GetLanguageModelParams();
  if (sampling_params) {
    sampling_params_ = optimization_guide::SamplingParams{
        .top_k = std::min(sampling_params->top_k,
                          language_model_params->max_sampling_params->top_k),
        .temperature =
            std::min(sampling_params->temperature,
                     language_model_params->max_sampling_params->temperature)};
  } else {
    sampling_params_ = optimization_guide::SamplingParams{
        .top_k = language_model_params->default_sampling_params->top_k,
        .temperature =
            language_model_params->default_sampling_params->temperature};
  }
}

CreateLanguageModelOnDeviceSessionTask::
    ~CreateLanguageModelOnDeviceSessionTask() = default;

void CreateLanguageModelOnDeviceSessionTask::OnFinish(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session) {
  std::move(completion_callback_).Run(std::move(session));
}

void CreateLanguageModelOnDeviceSessionTask::UpdateSessionConfigParams(
    optimization_guide::SessionConfigParams* config_params) {
  config_params->sampling_params = sampling_params_;
}
