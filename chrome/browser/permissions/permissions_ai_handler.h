// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSIONS_AI_HANDLER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSIONS_AI_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/permissions/request_type.h"

namespace permissions {

class PermissionsAiHandler
    : public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  explicit PermissionsAiHandler(
      OptimizationGuideKeyedService* optimization_guide);
  ~PermissionsAiHandler() override;
  PermissionsAiHandler(const PermissionsAiHandler&) = delete;
  PermissionsAiHandler& operator=(const PermissionsAiHandler&) = delete;

  bool IsOnDeviceModelAvailable();

  void InquireAiOnDeviceModel(
      std::string rendered_text,
      RequestType request_type,
      base::OnceCallback<
          void(std::optional<optimization_guide::proto::PermissionsAiResponse>)>
          callback);

 private:
  // Adds itself as OnDeviceModelAvailabilityObserver to the optimization guide
  // infrastructure.
  void StartListeningToOnDeviceModelUpdate();

  // Remove itself as OnDeviceModelAvailabilityObserver from the optimization
  // guide infrastructure, e.g. when the model is done downloading.
  void StopListeningToOnDeviceModelUpdate();

  // Initializes session_ field.
  void CreateModelExecutorSession();

  // optimization_guide::OnDeviceModelAvailabilityObserver.
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void SetOnDeviceModelAvailable();

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;

  void OnModelExecutionComplete(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // The underlying session provided by optimization guide component.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_;

  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  bool observing_on_device_model_availability_ = false;
  bool is_on_device_model_available_ = false;

  // Model downloading has begun at this point in time.
  base::TimeTicks on_device_download_start_time_;
  base::TimeTicks session_execution_start_time_;
  base::OnceCallback<void(
      std::optional<optimization_guide::proto::PermissionsAiResponse>)>
      inquire_on_device_model_callback_;
  base::WeakPtrFactory<PermissionsAiHandler> weak_ptr_factory_{this};
};
}  // namespace permissions

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSIONS_AI_HANDLER_H_
