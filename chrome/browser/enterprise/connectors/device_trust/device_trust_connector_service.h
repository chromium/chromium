// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;
class GURL;

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace enterprise_connectors {

// Keyed-Service in charge of monitoring the status of the Device Trust
// connector (e.g. enabled or not).
class DeviceTrustConnectorService : public KeyedService {
 public:
  explicit DeviceTrustConnectorService(PrefService* profile_prefs);

  DeviceTrustConnectorService(const DeviceTrustConnectorService&) = delete;
  DeviceTrustConnectorService& operator=(const DeviceTrustConnectorService&) =
      delete;

  ~DeviceTrustConnectorService() override;

  // Does one time initialization of the service.  This is called from the
  // factory and client do not need to call it.
  void Initialize();

  // Returns whether the Device Trust connector is enabled or not.
  virtual bool IsConnectorEnabled() const;

  // Returns whether the Device Trust connector watches navigations to the given
  // `url` or not.
  bool Watches(const GURL& url) const;

 protected:
  // Hook that can is called to notify that the policy changed and the connector
  // became, or is still, enabled.
  virtual void OnConnectorEnabled();

 private:
  // Called when the policy value changes in Prefs.
  void OnPolicyUpdated();

  PrefChangeRegistrar pref_observer_;

  raw_ptr<PrefService> profile_prefs_;

  // The URL matcher created from the ContextAwareAccessSignalsAllowlist policy.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  base::WeakPtrFactory<DeviceTrustConnectorService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_
