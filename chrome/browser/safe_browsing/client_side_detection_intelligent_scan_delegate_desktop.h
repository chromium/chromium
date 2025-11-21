// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
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
  std::optional<base::UnguessableToken> InquireOnDeviceModel(
      std::string rendered_texts,
      InquireOnDeviceModelDoneCallback callback) override;
  bool CancelSession(const base::UnguessableToken& session_id) override;
  bool ShouldShowScamWarning(
      std::optional<IntelligentScanVerdict> verdict) override;

  // KeyedService implementation.
  void Shutdown() override;

  int GetAliveSessionCountForTesting() { return inquiries_.size(); }

 private:
  friend class ClientSideDetectionIntelligentScanDelegateDesktopTest;
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionIntelligentScanDelegateDesktopTest,
      ResetOnDeviceSession);
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

  void ModelExecutionCallback(
      const base::UnguessableToken& session_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  bool ResetAllSessions();

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

  // PrefChangeRegistrar used to track when the enhanced protection state
  // changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ClientSideDetectionIntelligentScanDelegateDesktop>
      weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
