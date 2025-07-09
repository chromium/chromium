// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"

class PrefService;
class OptimizationGuideKeyedService;

namespace safe_browsing {

// Desktop implementation of IntelligentScanDelegate. This class is responsible
// for managing the on-device model for intelligent scanning, including loading,
// observing updates, and executing the model.
class ClientSideDetectionIntelligentScanDelegateDesktop
    : public ClientSideDetectionHost::IntelligentScanDelegate,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktop(
      PrefService& pref,
      OptimizationGuideKeyedService* opt_guide);
  ~ClientSideDetectionIntelligentScanDelegateDesktop() override;

  ClientSideDetectionIntelligentScanDelegateDesktop(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;
  ClientSideDetectionIntelligentScanDelegateDesktop& operator=(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;

  // IntelligentScanDelegate implementation.
  bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) override;
  bool IsOnDeviceModelAvailable(bool log_failed_eligibility_reason) override;
  void InquireOnDeviceModel(std::string rendered_texts,
                            InquireOnDeviceModelDoneCallback callback) override;
  bool ResetOnDeviceSession() override;

  // KeyedService implementation.
  void Shutdown() override;

  bool IsSessionAliveForTesting() { return !!session_; }

 private:
  void OnPrefsUpdated();

  // Starts listening to the on-device model update through OptimizationGuide.
  // This will be called when the user preferences change and the user is
  // subscribed to Enhanced Safe Browsing. Does nothing if it is already
  // listening to the on-device model update.
  void StartListeningToOnDeviceModelUpdate();
  // Stops listening to the on-device model update through OptimizationGuide.
  // Does nothing if it is not listening to the on-device model update.
  void StopListeningToOnDeviceModelUpdate();

  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void NotifyOnDeviceModelAvailable();

  void LogOnDeviceModelEligibilityReason();

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
  GetModelExecutorSession();

  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  bool observing_on_device_model_availability_ = false;
  // This is used to check before fetching the session when the correct trigger
  // is called to generate the on-device model LLM.
  bool on_device_model_available_ = false;
  base::TimeTicks on_device_fetch_time_;

  base::TimeTicks session_execution_start_time_;
  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  InquireOnDeviceModelDoneCallback inquire_on_device_model_callback_;

  const raw_ref<PrefService> pref_;
  const raw_ptr<OptimizationGuideKeyedService> opt_guide_;

  // PrefChangeRegistrar used to track when the enhanced protection state
  // changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ClientSideDetectionIntelligentScanDelegateDesktop>
      weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
