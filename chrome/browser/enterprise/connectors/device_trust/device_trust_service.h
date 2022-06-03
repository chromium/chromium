// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace enterprise_connectors {

class AttestationService;
class SignalsService;

// Main service used to drive device trust connector scenarios. It is currently
// used to generate a response for a crypto challenge received from Verified
// Access during an attestation flow.
class DeviceTrustService : public KeyedService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

  using TrustedUrlPatternsChangedCallbackList =
      base::RepeatingCallbackList<void(const base::ListValue&)>;
  using TrustedUrlPatternsChangedCallback =
      TrustedUrlPatternsChangedCallbackList::CallbackType;

  // Check if DeviceTrustService is enabled via prefs with non-empty allowlist.
  static bool IsEnabled(PrefService* prefs);

  DeviceTrustService(PrefService* profile_prefs,
                     std::unique_ptr<AttestationService> attestation_service,
                     std::unique_ptr<SignalsService> signals_service);

  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

  ~DeviceTrustService() override;

  // Check if DeviceTrustService is enabled.  This method may be called from
  // any task sequence.
  virtual bool IsEnabled() const;

  // Starts flow that actually builds a response.
  virtual void BuildChallengeResponse(const std::string& challenge,
                                      AttestationCallback callback);

  // Collects device trust signals and returns them via `callback`.
  void GetSignals(
      base::OnceCallback<void(std::unique_ptr<SignalsType>)> callback);

  // Register a `callback` that listens for changes in the trust URL patterns.
  // The callback may be run synchronously for initialization purposes.
  virtual base::CallbackListSubscription
  RegisterTrustedUrlPatternsChangedCallback(
      TrustedUrlPatternsChangedCallback callback);

  // KeyedService:
  void Shutdown() override;

 protected:
  // Default constructor that can be used by mocks to bypass initialization.
  explicit DeviceTrustService(PrefService* profile_prefs);

 private:
  void OnPolicyUpdated();

  void OnSignalsCollected(const std::string& challenge,
                          AttestationCallback callback,
                          std::unique_ptr<SignalsType> signals);

  PrefChangeRegistrar pref_observer_;

  PrefService* const profile_prefs_;
  std::unique_ptr<AttestationService> attestation_service_;
  std::unique_ptr<SignalsService> signals_service_;
  TrustedUrlPatternsChangedCallbackList callbacks_;

  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
