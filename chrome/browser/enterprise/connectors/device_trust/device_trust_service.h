// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

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

  Profile* profile_;
  PrefService* prefs_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
