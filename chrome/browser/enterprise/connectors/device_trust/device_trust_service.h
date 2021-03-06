// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include "base/memory/checked_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

class Profile;
class PrefService;

namespace policy {

class DeviceTrustService : public KeyedService {
 public:
  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

 private:
  friend class DeviceTrustFactory;

  DeviceTrustService();
  explicit DeviceTrustService(Profile* profile);
  ~DeviceTrustService() override;

  CheckedPtr<Profile> profile_;
  CheckedPtr<PrefService> prefs_;
  std::unique_ptr<DeviceTrustKeyPair> key_pair_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
