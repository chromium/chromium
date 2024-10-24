// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_
#define CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_

#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-forward.h"

// A base class for tasks which create an on-device session. See the method
// comment of `Run()` for the details.
class CreateOnDeviceSessionTask
    : public AIContextBoundObject,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  CreateOnDeviceSessionTask(
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature);
  ~CreateOnDeviceSessionTask() override;
  CreateOnDeviceSessionTask(const CreateOnDeviceSessionTask&) = delete;
  CreateOnDeviceSessionTask& operator=(const CreateOnDeviceSessionTask&) =
      delete;

  bool observing_availability() const { return observing_availability_; }

  // Attempts to create an on-device session.
  //
  // * If `service_` is null, immediately calls `OnFinish()` with a nullptr,
  //   indicating failure.
  // * If creation succeeds, calls `OnFinish()` with the newly created session.
  // * If creation fails:
  //   * If the failure reason is in `kWaitableReasons` (indicating a
  //     potentially temporary issue):
  //     * Registers itself to observe model availability changes in `service_`.
  //     * Waits until the `reason` is no longer in `kWaitableReasons`, then
  //       retries session creation.
  //     * Updates the `observing_availability_` to true.
  //   * Otherwise (for non-recoverable errors), calls `OnFinish()` with a
  //     nullptr.
  void Run();

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
  // `AIContextBoundObject` implementation.
  void SetDeletionCallback(base::OnceClosure deletion_callback) override;

  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
  StartSession();

  OptimizationGuideKeyedService* GetOptimizationGuideService();

  const raw_ptr<content::BrowserContext> browser_context_;
  const optimization_guide::ModelBasedCapabilityKey feature_;
  // The state indicates if the current `CreateOnDeviceSessionTask` is pending.
  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  // If this is true, the `CreateOnDeviceSessionTas` should be kept alive as it
  // needs to keep observing the on-device model availability.
  bool observing_availability_ = false;
  base::OnceClosure deletion_callback_;
};

// Implementation of the `CreateOnDeviceSessionTask` base class for AIAssistant.
class CreateAssistantOnDeviceSessionTask : public CreateOnDeviceSessionTask {
 public:
  CreateAssistantOnDeviceSessionTask(
      content::BrowserContext* browser_context,
      const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
      base::OnceCallback<
          void(std::unique_ptr<
               optimization_guide::OptimizationGuideModelExecutor::Session>)>
          completion_callback);
  ~CreateAssistantOnDeviceSessionTask() override;

  CreateAssistantOnDeviceSessionTask(
      const CreateAssistantOnDeviceSessionTask&) = delete;
  CreateAssistantOnDeviceSessionTask& operator=(
      const CreateAssistantOnDeviceSessionTask&) = delete;

 protected:
  // `CreateOnDeviceSessionTask` implementation.
  void OnFinish(std::unique_ptr<
                optimization_guide::OptimizationGuideModelExecutor::Session>
                    session) override;

  void UpdateSessionConfigParams(
      optimization_guide::SessionConfigParams* config_params) override;

 private:
  std::optional<optimization_guide::SamplingParams> sampling_params_ =
      std::nullopt;
  base::OnceCallback<void(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session>)>
      completion_callback_;
};

#endif  // CHROME_BROWSER_AI_AI_CREATE_ON_DEVICE_SESSION_TASK_H_
