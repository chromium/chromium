// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/mac_device_trust_connector_service.h"

#include "chrome/browser/enterprise/connectors/device_trust/browser/browser_device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

MacDeviceTrustConnectorService::MacDeviceTrustConnectorService(
    DeviceTrustKeyManager* key_manager,
    PrefService* profile_prefs,
    PrefService* local_prefs)
    : BrowserDeviceTrustConnectorService(key_manager, profile_prefs),
      local_prefs_(local_prefs) {
  DCHECK(local_prefs_);
}

MacDeviceTrustConnectorService::~MacDeviceTrustConnectorService() = default;

bool MacDeviceTrustConnectorService::IsConnectorEnabled() const {
  return BrowserDeviceTrustConnectorService::IsConnectorEnabled() &&
         !local_prefs_->GetBoolean(kDeviceTrustDisableKeyCreationPref);
}

void MacDeviceTrustConnectorService::OnConnectorEnabled() {
  if (!local_prefs_->GetBoolean(kDeviceTrustDisableKeyCreationPref)) {
    BrowserDeviceTrustConnectorService::OnConnectorEnabled();
  }
}

}  // namespace enterprise_connectors
