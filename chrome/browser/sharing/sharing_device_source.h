// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

class SharingDeviceSource {
 public:
  SharingDeviceSource();
  virtual ~SharingDeviceSource();

  // Returns if the source is ready. Calling GetAllDevices before this is true
  // returns an empty list.
  virtual bool IsReady() = 0;

  // Returns the device matching |guid|, or nullptr if no match was found.
  virtual std::unique_ptr<syncer::DeviceInfo> GetDeviceByGuid(
      const std::string& guid) = 0;

  // Returns all devices found.
  virtual std::vector<std::unique_ptr<syncer::DeviceInfo>> GetAllDevices() = 0;

  void AddReadyCallback(base::OnceClosure callback);

 protected:
  void MaybeRunReadyCallbacks();

 private:
  std::vector<base::OnceClosure> ready_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SharingDeviceSource);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_
