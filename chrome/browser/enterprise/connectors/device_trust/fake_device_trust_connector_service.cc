// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/fake_device_trust_connector_service.h"

#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace enterprise_connectors {

FakeDeviceTrustConnectorService::FakeDeviceTrustConnectorService(
    sync_preferences::TestingPrefServiceSyncable* profile_prefs)
    : DeviceTrustConnectorService(profile_prefs), test_prefs_(profile_prefs) {}

FakeDeviceTrustConnectorService::~FakeDeviceTrustConnectorService() = default;

void FakeDeviceTrustConnectorService::update_policy(
    base::Value::List new_urls) {
  test_prefs_->SetManagedPref(kContextAwareAccessSignalsAllowlistPref,
                              base::Value(std::move(new_urls)));
}

}  // namespace enterprise_connectors
