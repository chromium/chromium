// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_
#define CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_

#include "base/state_transitions.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "services/on_device_model/public/cpp/capabilities.h"

// A base class for tasks which create an on-device session.
class CreateOnDeviceSessionTask
    : public AIContextBoundObject,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  CreateOnDeviceSessionTask(
      AIContextBoundObjectSet& context_bound_object_set,
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature);
  ~CreateOnDeviceSessionTask() override;
  CreateOnDeviceSessionTask(const CreateOnDeviceSessionTask&) = delete;
  CreateOnDeviceSessionTask& operator=(const CreateOnDeviceSessionTask&) =
      delete;

  bool IsPending() const { return state_ == State::kPending; }

  // Starts the process of creating an on-device model session.
  // It may succeed or fail immediately, or it may move into the `kPending`
  // state if it needs to wait for the on-device model availability changes.
  // See `kWaitableReasons` for more details.
  void Start();

 protected:
  // Cancels the creation task, and deletes itself.
  void Cancel();

  virtual void OnFinish(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session>
          session) = 0;

  virtual void UpdateSessionConfigParams(
      optimization_guide::SessionConfigParams* config_params) {}

 private:
  // The state of `CreateOnDeviceSessionTask`.
  // The possible transitions of state are:
  // - kNotStarted -> kFinished
  // - kNotStarted -> kPending
  // - kPending -> kFinished
  // - kPending -> kCancelled
  enum class State {
    // The task is created but not started yet.
    kNotStarted,
    // The task has started, but the on-device model is not readily available
    // and is expected to be ready soon.
    // When the task is in `kPending` state, it should be kept alive by the
    // creator as it needs to keep observing the on-device model availability
    // changes.
    // See `kWaitableReasons` for more details.
    kPending,
    // The task is finished, but it's not guaranteed that the session has been
    // created successfully.
    kFinished,
    // The task is cancelled before finishing.
    kCancelled,
  };

  friend base::StateTransitions<State>;
  friend std::ostream& operator<<(std::ostream& os, State state);

  void SetState(State state);

  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void OnGetEligibility(
      optimization_guide::OnDeviceModelEligibilityReason eligibility);

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
  StartSession();

  void Finish(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session);

  OptimizationGuideKeyedService* GetOptimizationGuideService();

  const raw_ptr<content::BrowserContext> browser_context_;
  const optimization_guide::ModelBasedCapabilityKey feature_;
  State state_ = CreateOnDeviceSessionTask::State::kNotStarted;
  base::WeakPtrFactory<CreateOnDeviceSessionTask> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_
