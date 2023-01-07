// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_BROWSER_DEVICE_TRUST_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_BROWSER_DEVICE_TRUST_CONNECTOR_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

class PrefService;

namespace enterprise_connectors {

class DeviceTrustKeyManager;

// Browser-specific service in charge of monitoring the status of the Device
// Trust connector (e.g. enabled or not).
class BrowserDeviceTrustConnectorService : public DeviceTrustConnectorService {
 public:
  explicit BrowserDeviceTrustConnectorService(
      DeviceTrustKeyManager* key_manager,
      PrefService* profile_prefs);

  BrowserDeviceTrustConnectorService(
      const BrowserDeviceTrustConnectorService&) = delete;
  BrowserDeviceTrustConnectorService& operator=(
      const BrowserDeviceTrustConnectorService&) = delete;

  ~BrowserDeviceTrustConnectorService() override;

 protected:
  // Hook that is called to notify that the policy changed and the connector
  // became, or is still, enabled.
  void OnConnectorEnabled() override;

 private:
  raw_ptr<DeviceTrustKeyManager> key_manager_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_BROWSER_DEVICE_TRUST_CONNECTOR_SERVICE_H_
