// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_DRIVER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_DRIVER_H_

#include <memory>
#include <string>

#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {
class InstanceID;
}  // namespace instance_id

class FakeSyncInstanceID;

class FakeSyncInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  explicit FakeSyncInstanceIDDriver(gcm::GCMDriver* gcm_driver);

  FakeSyncInstanceIDDriver(const FakeSyncInstanceIDDriver&) = delete;
  FakeSyncInstanceIDDriver& operator=(const FakeSyncInstanceIDDriver&) = delete;

  ~FakeSyncInstanceIDDriver() override;
  instance_id::InstanceID* GetInstanceID(const std::string& app_id) override;
  void RemoveInstanceID(const std::string& app_id) override {}
  bool ExistsInstanceID(const std::string& app_id) const override;

 private:
  raw_ptr<gcm::GCMDriver, DanglingUntriaged> gcm_driver_;
  std::map<std::string, std::unique_ptr<FakeSyncInstanceID>> fake_instance_ids_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_DRIVER_H_
