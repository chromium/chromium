// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/sync/protocol/device_info_specifics.pb.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

class SharingDeviceSource {
 public:
  SharingDeviceSource();

  SharingDeviceSource(const SharingDeviceSource&) = delete;
  SharingDeviceSource& operator=(const SharingDeviceSource&) = delete;

  virtual ~SharingDeviceSource();

  // Returns if the source is ready. Calling GetAllDevices before this is true
  // returns an empty list.
  virtual bool IsReady() = 0;

  // Returns the device matching |guid|, or nullptr if no match was found.
  virtual std::unique_ptr<syncer::DeviceInfo> GetDeviceByGuid(
      const std::string& guid) = 0;

  // Returns all device candidates for |required_feature|. Internally filters
  // out older devices and returns them in (not strictly) decreasing order of
  // last updated timestamp.
  virtual std::vector<std::unique_ptr<syncer::DeviceInfo>> GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) = 0;

  // Adds a callback to be run when the SharingDeviceSource is ready. If a
  // callback is added when it is already ready, it will be run immediately.
  void AddReadyCallback(base::OnceClosure callback);

 protected:
  void MaybeRunReadyCallbacks();

 private:
  std::vector<base::OnceClosure> ready_callbacks_;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_SOURCE_H_
