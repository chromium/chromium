// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"

class PrefService;
class OptimizationGuideKeyedService;

namespace safe_browsing {

// Desktop implementation of IntelligentScanDelegate. This class is responsible
// for managing the on-device model for intelligent scanning, including loading,
// observing updates, and executing the model.
// TODO(crbug.com/424104358): Move remaining functions into this class.
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
  void StartListeningToOnDeviceModelUpdate() override;
  void StopListeningToOnDeviceModelUpdate() override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void NotifyOnDeviceModelAvailable();

  void LogOnDeviceModelEligibilityReason();

  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  bool observing_on_device_model_availability_ = false;
  // This is used to check before fetching the session when the correct trigger
  // is called to generate the on-device model LLM.
  bool on_device_model_available_ = false;
  base::TimeTicks on_device_fetch_time_;

  const raw_ref<PrefService> pref_;
  const raw_ptr<OptimizationGuideKeyedService> opt_guide_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
