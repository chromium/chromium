// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_

#include "chrome/browser/ash/policy/external_data/device_cloud_external_data_policy_observer.h"

namespace policy {

// Base class for handling per-device external resources like wallpaper or
// printers configuration.
class DeviceCloudExternalDataPolicyHandler
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  DeviceCloudExternalDataPolicyHandler();

  DeviceCloudExternalDataPolicyHandler(
      const DeviceCloudExternalDataPolicyHandler&) = delete;
  DeviceCloudExternalDataPolicyHandler& operator=(
      const DeviceCloudExternalDataPolicyHandler&) = delete;

  virtual void Shutdown() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
