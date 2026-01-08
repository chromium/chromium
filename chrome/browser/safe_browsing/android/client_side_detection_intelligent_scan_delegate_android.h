// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

class PrefService;

namespace optimization_guide {
class ModelBrokerClient;
}  // namespace optimization_guide

namespace safe_browsing {

// Client Side Detection Android implementation of IntelligentScanDelegate. This
// class is responsible for managing intelligent scan inquiries and executing
// the model.
class ClientSideDetectionIntelligentScanDelegateAndroid
    : public IntelligentScanDelegate {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroid(
      PrefService& pref,
      std::unique_ptr<optimization_guide::ModelBrokerClient>
          model_broker_client,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  ~ClientSideDetectionIntelligentScanDelegateAndroid() override;

  ClientSideDetectionIntelligentScanDelegateAndroid(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;
  ClientSideDetectionIntelligentScanDelegateAndroid& operator=(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;

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
  void SetPauseInquiryForTesting(bool pause) {
    pause_inquiry_for_testing_ = pause;
  }

 private:
  class Inquiry;
  bool ResetAllInquiries();

  void OnPrefsUpdated();

  // Starts on-device model download.
  void StartModelDownload();

  // Functions related to intelligent scan quota:
  // Returns true if we have reached the quota limit. Also clears the expired
  // timestamps.
  bool IsAtIntelligentScanQuota();
  void AddIntelligentScanQuota();
  void RemoveLastIntelligentScanQuota();

  const raw_ref<PrefService> pref_;
  // This object is used to download the model and create sessions for on-device
  // model execution. It may be null after shutdown.
  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;
  // This object is for server-side model execution. It may be null after
  // shutdown.
  raw_ptr<optimization_guide::RemoteModelExecutor> remote_model_executor_;

  // A wrapper of the current intelligent scan inquiries. This is null if there
  // is no active inquiry.
  base::flat_map<base::UnguessableToken, std::unique_ptr<Inquiry>> inquiries_;

  // PrefChangeRegistrar used to track when the enhanced protection state
  // changes.
  PrefChangeRegistrar pref_change_registrar_;

  const bool is_feature_enabled_;
  const bool is_server_model_enabled_;

  bool pause_inquiry_for_testing_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_
