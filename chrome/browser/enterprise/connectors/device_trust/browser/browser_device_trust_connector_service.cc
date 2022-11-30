// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/browser_device_trust_connector_service.h"

#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

BrowserDeviceTrustConnectorService::BrowserDeviceTrustConnectorService(
    DeviceTrustKeyManager* key_manager,
    PrefService* profile_prefs)
    : DeviceTrustConnectorService(profile_prefs), key_manager_(key_manager) {
  DCHECK(key_manager_);
}

BrowserDeviceTrustConnectorService::~BrowserDeviceTrustConnectorService() =
    default;

void BrowserDeviceTrustConnectorService::OnConnectorEnabled() {
  key_manager_->StartInitialization();
}

}  // namespace enterprise_connectors
