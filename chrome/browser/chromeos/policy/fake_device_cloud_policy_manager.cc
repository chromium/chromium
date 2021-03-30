// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_manager.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"

namespace policy {

FakeDeviceCloudPolicyManager::FakeDeviceCloudPolicyManager(
    std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : DeviceCloudPolicyManagerChromeOS(
          std::move(store),
          std::make_unique<MockCloudExternalDataManager>(),
          task_runner,
          nullptr),
      unregister_result_(true) {}

FakeDeviceCloudPolicyManager::~FakeDeviceCloudPolicyManager() {
  Shutdown();
}

void FakeDeviceCloudPolicyManager::Unregister(UnregisterCallback callback) {
  std::move(callback).Run(unregister_result_);
}

void FakeDeviceCloudPolicyManager::Disconnect() {
}

}  // namespace policy
