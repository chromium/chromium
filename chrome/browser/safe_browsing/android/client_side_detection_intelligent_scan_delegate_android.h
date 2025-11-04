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
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"

class PrefService;

namespace optimization_guide {
class ModelBrokerClient;
}  // namespace optimization_guide

namespace safe_browsing {

// Android implementation of IntelligentScanDelegate. This class is responsible
// for managing sessions and executing the model.
class ClientSideDetectionIntelligentScanDelegateAndroid
    : public ClientSideDetectionHost::IntelligentScanDelegate {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroid(
      PrefService& pref,
      std::unique_ptr<optimization_guide::ModelBrokerClient>
          model_broker_client);
  ~ClientSideDetectionIntelligentScanDelegateAndroid() override;

  ClientSideDetectionIntelligentScanDelegateAndroid(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;
  ClientSideDetectionIntelligentScanDelegateAndroid& operator=(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;

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
  void SetPauseSessionExecutionForTesting(bool pause) {
    pause_session_execution_for_testing_ = pause;
  }

 private:
  class Inquiry;
  bool ResetAllSessions();

  void OnPrefsUpdated();

  // Starts on-device model download.
  void StartModelDownload();

  const raw_ref<PrefService> pref_;
  // This object is used to download the model and create sessions.
  // It may be null after shutdown.
  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;

  // A wrapper of the current on-device model session. This is null if there is
  // no active inquiry.
  base::flat_map<base::UnguessableToken, std::unique_ptr<Inquiry>> inquiries_;

  // PrefChangeRegistrar used to track when the enhanced protection state
  // changes.
  PrefChangeRegistrar pref_change_registrar_;

  bool pause_session_execution_for_testing_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_
