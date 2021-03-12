// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#include <memory>

class Profile;
class PrefService;
namespace enterprise_connectors {
class DeviceTrustSignalReporter;
}

namespace policy {

class DeviceTrustService : public KeyedService {
 public:
  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

  // Check if DeviceTrustService is enabled via prefs with non-empty allowlist.
  bool IsEnabled() const;

  // These methods are added to facilitate testing, because this class is
  // usually created by its factory.
  void SetSignalReporterForTesting(
      std::unique_ptr<enterprise_connectors::DeviceTrustSignalReporter>
          reporter);
  void SetSignalReportCallbackForTesting(base::OnceCallback<void(bool)> cb);

 private:
  friend class DeviceTrustFactory;

  DeviceTrustService();
  explicit DeviceTrustService(Profile* profile);
  ~DeviceTrustService() override;

  void Shutdown() override;

  void OnPolicyUpdated();
  void OnReporterInitialized(bool success);
  void OnSignalReported(bool success);

  PrefService* prefs_;
  std::unique_ptr<DeviceTrustKeyPair> key_pair_;

  PrefChangeRegistrar pref_observer_;
  bool first_report_sent_;

  std::unique_ptr<enterprise_connectors::DeviceTrustSignalReporter> reporter_;
  base::OnceCallback<void(bool)> signal_report_callback_;
  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
