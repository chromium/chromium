// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include "base/callback_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/attestation_service.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

#include <memory>

class KeyedService;
class Profile;
class PrefService;
namespace enterprise_connectors {
class DeviceTrustSignalReporter;
}

namespace enterprise_connectors {

class DeviceTrustService : public KeyedService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

  using TrustedUrlPatternsChangedCallbackList =
      base::RepeatingCallbackList<void(const base::ListValue*)>;
  using TrustedUrlPatternsChangedCallback =
      TrustedUrlPatternsChangedCallbackList::CallbackType;

  // Check if DeviceTrustService is enabled via prefs with non-empty allowlist.
  static bool IsEnabled(PrefService* prefs);

  DeviceTrustService() = delete;
  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;
  ~DeviceTrustService() override;

  // Check if DeviceTrustService is enabled.  This method may be called from
  // any task sequence.
  bool IsEnabled() const;

  // These methods are added to facilitate testing, because this class is
  // usually created by its factory.
  void SetSignalReporterForTesting(
      std::unique_ptr<DeviceTrustSignalReporter> reporter);
  using SignalReportCallback = base::OnceCallback<void(bool)>;
  void SetSignalReportCallbackForTesting(SignalReportCallback cb);

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  std::string GetAttestationCredentialForTesting() const;
  // Starts flow that actually builds a response. This method is called
  // from a non_UI thread.
  void BuildChallengeResponse(const std::string& challenge,
                              AttestationCallback callback);
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  // Register a callback that listens for changes in the trust URL patterns.
  base::CallbackListSubscription RegisterTrustedUrlPatternsChangedCallback(
      TrustedUrlPatternsChangedCallback callback);

 private:
  friend class DeviceTrustFactory;

  explicit DeviceTrustService(Profile* profile);

  void Shutdown() override;

  void OnPolicyUpdated();
  void OnReporterInitialized(bool success);
  void OnSignalReported(bool success);

  base::RepeatingCallback<bool()> MakePolicyCheck();

  PrefService* prefs_;

  // Caches whether the device trust service is enabled or not.  This is used
  // to implement IsEnabled() so the method does not need to access the prefs.
  // This is important because |reporter_| will indirectly call IsEnabled()
  // from a sequence that cannot call prefs methods.
  bool is_enabled_ = false;

  PrefChangeRegistrar pref_observer_;
  bool first_report_sent_ = false;

  std::unique_ptr<enterprise_connectors::DeviceTrustSignalReporter> reporter_;
  SignalReportCallback signal_report_callback_;

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  std::unique_ptr<AttestationService> attestation_service_;
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  TrustedUrlPatternsChangedCallbackList callbacks_;

  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
