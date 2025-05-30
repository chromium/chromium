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
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace {

bool IsWaitableReason(
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  auto availability =
      optimization_guide::AvailabilityFromEligibilityReason(reason);
  if (!availability.has_value()) {
    // The model should be available (reason == kSuccess), so we shouldn't wait.
    return false;
  }
  using optimization_guide::mojom::ModelUnavailableReason;
  switch (availability.value()) {
    case ModelUnavailableReason::kNotSupported:
      return false;
    case ModelUnavailableReason::kPendingAssets:
      // Model / lora / config needs to finish downloading / verifying etc.
      return true;
    case ModelUnavailableReason::kPendingUsage:
      // This doesn't resolve by waiting, the model would need to be requested.
      // We shouldn't hit this once we've requested the model though.
      return false;
    case ModelUnavailableReason::kUnknown:
      // We don't expect to hit this, since we've waited for the controller to
      // initialize. It would resolve by waiting though.
      return true;
  }
}

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

  SetState(State::kPending);
  service->GetOnDeviceModelEligibilityAsync(
      feature_, /*capabilities=*/{},
      base::BindOnce(&CreateOnDeviceSessionTask::OnGetEligibility,
                     weak_factory_.GetWeakPtr()));
}

void CreateOnDeviceSessionTask::OnGetEligibility(
    optimization_guide::OnDeviceModelEligibilityReason eligibility) {
  OptimizationGuideKeyedService* service = GetOptimizationGuideService();
  if (!service) {
    Finish(nullptr);
    return;
  }

  if (auto session = StartSession()) {
    Finish(std::move(session));
    return;
  }
  CHECK_NE(eligibility,
           optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  if (!IsWaitableReason(eligibility)) {
    BUILT_IN_AI_LOGGER() << "Cannot create session for feature '" << feature_
                         << "'. " << "Reason: " << eligibility;
    Finish(nullptr);
    return;
  }
  service->AddOnDeviceModelAvailabilityChangeObserver(feature_, this);
}

void CreateOnDeviceSessionTask::Cancel() {
  SetState(State::kCancelled);
  RemoveFromSet();
}

void CreateOnDeviceSessionTask::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  bool waitable = IsWaitableReason(reason);
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
