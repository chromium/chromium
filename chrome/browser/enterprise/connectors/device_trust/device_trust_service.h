// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include "base/callback_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

#include <memory>

class KeyedService;
class PrefService;

namespace enterprise_connectors {

class DeviceTrustSignalReporter;

class DeviceTrustService : public KeyedService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;
  using SignalReportCallback = base::OnceCallback<void(bool)>;

  using TrustedUrlPatternsChangedCallbackList =
      base::RepeatingCallbackList<void(const base::ListValue*)>;
  using TrustedUrlPatternsChangedCallback =
      TrustedUrlPatternsChangedCallbackList::CallbackType;

  // Check if DeviceTrustService is enabled via prefs with non-empty allowlist.
  static bool IsEnabled(PrefService* prefs);

  explicit DeviceTrustService(
      PrefService* pref_service,
      std::unique_ptr<AttestationService> attestation_service,
      std::unique_ptr<DeviceTrustSignalReporter> signal_reporter);
  explicit DeviceTrustService(
      PrefService* pref_service,
      std::unique_ptr<AttestationService> attestation_service,
      std::unique_ptr<DeviceTrustSignalReporter> signal_reporter,
      SignalReportCallback signal_report_callback);
  ~DeviceTrustService() override;

  // Not copyable or movable.
  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

  // Check if DeviceTrustService is enabled.  This method may be called from
  // any task sequence.
  bool IsEnabled() const;

  // Starts flow that actually builds a response.
  void BuildChallengeResponse(const std::string& challenge,
                              AttestationCallback callback);

  // Register a callback that listens for changes in the trust URL patterns.
  base::CallbackListSubscription RegisterTrustedUrlPatternsChangedCallback(
      TrustedUrlPatternsChangedCallback callback);

 private:
  void Shutdown() override;

  void OnPolicyUpdated();
  void OnReporterInitialized(bool success);
  void OnSignalReported(bool success);

  base::RepeatingCallback<bool()> MakePolicyCheck();

  // Caches whether the device trust service is enabled or not.  This is used
  // to implement IsEnabled() so the method does not need to access the prefs.
  // This is important because |reporter_| will indirectly call IsEnabled()
  // from a sequence that cannot call prefs methods.
  bool is_enabled_ = false;

  PrefChangeRegistrar pref_observer_;
  bool first_report_sent_ = false;

  PrefService* prefs_;
  std::unique_ptr<AttestationService> attestation_service_;
  std::unique_ptr<DeviceTrustSignalReporter> signal_reporter_;
  SignalReportCallback signal_report_callback_;
  TrustedUrlPatternsChangedCallbackList callbacks_;

  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
