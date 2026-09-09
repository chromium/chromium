// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

class PrefService;
class OptimizationGuideKeyedService;

namespace policy {
class ManagementService;
}

namespace safe_browsing {

// Client Side Detection Desktop implementation of IntelligentScanDelegate. This
// class is responsible for managing the on-device model for intelligent
// scanning, including loading, observing updates, and executing the model.
// TODO(b/546066852): Refactor common logic between Desktop & Android delegate
// into IntelligentScanDelegate
class ClientSideDetectionIntelligentScanDelegateDesktop
    : public IntelligentScanDelegate,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktop(
      PrefService& pref,
      OptimizationGuideKeyedService* opt_guide,
      policy::ManagementService* management_service,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  ~ClientSideDetectionIntelligentScanDelegateDesktop() override;

  ClientSideDetectionIntelligentScanDelegateDesktop(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;
  ClientSideDetectionIntelligentScanDelegateDesktop& operator=(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;

  // IntelligentScanDelegate implementation.
  bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) override;
  ModelType GetIntelligentScanModelType(
      bool log_failed_eligibility_reason) override;
  std::optional<base::UnguessableToken> StartIntelligentScan(
      std::string rendered_texts,
      IntelligentScanDoneCallback callback) override;
  bool CancelIntelligentScan(const base::UnguessableToken& scan_id) override;
  bool ShouldShowScamWarning(
      std::optional<IntelligentScanVerdict> verdict) override;
  void OnScamWarningShown() override;

  // KeyedService implementation.
  void Shutdown() override;

  int GetAliveInquiryCountForTesting() { return inquiries_.size(); }

 private:
  class Inquiry;
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
      optimization_guide::mojom::OnDeviceFeature feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void NotifyOnDeviceModelAvailable();

  void LogOnDeviceModelEligibilityReason();

  std::unique_ptr<optimization_guide::OnDeviceSession>
  GetModelExecutorSession();

  bool ResetAllInquiries();

  // Functions related to intelligent scan quota:
  // Returns true if we have reached the quota limit. Also clears the expired
  // timestamps.
  bool IsAtIntelligentScanQuota();
  void AddIntelligentScanQuota();
  void RemoveLastIntelligentScanQuota();

  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  bool observing_on_device_model_availability_ = false;
  // This is used to check before fetching the session when the correct trigger
  // is called to generate the on-device model LLM.
  bool on_device_model_available_ = false;
  base::TimeTicks on_device_fetch_time_;

  // A wrapper of the current on-device model session. This is null if there is
  // no active inquiry.
  base::flat_map<base::UnguessableToken, std::unique_ptr<Inquiry>> inquiries_;

  const raw_ref<PrefService> pref_;
  const raw_ptr<OptimizationGuideKeyedService> opt_guide_;
  const raw_ptr<policy::ManagementService> management_service_;
  // This object is for server-side model execution. It may be null after
  // shutdown.
  raw_ptr<optimization_guide::RemoteModelExecutor> remote_model_executor_;

  // PrefChangeRegistrar used to track when the enhanced protection state
  // changes.
  PrefChangeRegistrar pref_change_registrar_;

  const bool is_feature_enabled_;
  const bool is_server_model_enabled_;

  base::WeakPtrFactory<ClientSideDetectionIntelligentScanDelegateDesktop>
      weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
