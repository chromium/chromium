// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_MAC_DEVICE_TRUST_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_MAC_DEVICE_TRUST_CONNECTOR_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/browser/browser_device_trust_connector_service.h"

class PrefService;

namespace enterprise_connectors {

class DeviceTrustKeyManager;

// Mac implementation of the browser device trust connector service in charge of
// monitoring the status of the Device Trust connector (e.g. enabled or not) on
// Mac platforms.
class MacDeviceTrustConnectorService
    : public BrowserDeviceTrustConnectorService {
 public:
  MacDeviceTrustConnectorService(DeviceTrustKeyManager* key_manager,
                                 PrefService* profile_prefs,
                                 PrefService* local_prefs);

  MacDeviceTrustConnectorService(const MacDeviceTrustConnectorService&) =
      delete;
  MacDeviceTrustConnectorService& operator=(
      const MacDeviceTrustConnectorService&) = delete;

  ~MacDeviceTrustConnectorService() override;

  // Returns whether the Device Trust connector is enabled or not.
  bool IsConnectorEnabled() const override;

 private:
  base::raw_ptr<PrefService> local_prefs_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_MAC_DEVICE_TRUST_CONNECTOR_SERVICE_H_
