// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_

#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

namespace policy {

// Base class for handling per-device external resources like wallpaper or
// printers configuration.
class DeviceCloudExternalDataPolicyHandler
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  DeviceCloudExternalDataPolicyHandler();

  virtual void Shutdown() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudExternalDataPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
