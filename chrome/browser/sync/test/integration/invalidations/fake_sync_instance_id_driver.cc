// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/invalidations/fake_sync_instance_id_driver.h"

#include "chrome/browser/sync/test/integration/invalidations/fake_sync_instance_id.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"

FakeSyncInstanceIDDriver::FakeSyncInstanceIDDriver(gcm::GCMDriver* gcm_driver)
    : instance_id::InstanceIDDriver(gcm_driver), gcm_driver_(gcm_driver) {}

FakeSyncInstanceIDDriver::~FakeSyncInstanceIDDriver() = default;

instance_id::InstanceID* FakeSyncInstanceIDDriver::GetInstanceID(
    const std::string& app_id) {
  if (!fake_instance_ids_.count(app_id)) {
    fake_instance_ids_[app_id] =
        std::make_unique<FakeSyncInstanceID>(app_id, gcm_driver_);
  }
  return fake_instance_ids_[app_id].get();
}

bool FakeSyncInstanceIDDriver::ExistsInstanceID(
    const std::string& app_id) const {
  return fake_instance_ids_.count(app_id);
}
