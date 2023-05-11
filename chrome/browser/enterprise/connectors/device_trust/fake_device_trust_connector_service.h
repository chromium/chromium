// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FAKE_DEVICE_TRUST_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FAKE_DEVICE_TRUST_CONNECTOR_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}  // namespace sync_preferences

namespace enterprise_connectors {

class FakeDeviceTrustConnectorService : public DeviceTrustConnectorService {
 public:
  explicit FakeDeviceTrustConnectorService(
      sync_preferences::TestingPrefServiceSyncable* profile_prefs);
  ~FakeDeviceTrustConnectorService() override;

  void UpdateInlinePolicy(base::Value::List new_urls,
                          DTCPolicyLevel policy_level);

 private:
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> test_prefs_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FAKE_DEVICE_TRUST_CONNECTOR_SERVICE_H_
