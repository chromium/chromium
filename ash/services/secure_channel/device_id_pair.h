// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_
#define ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_

#include <ostream>
#include <string>
#include <unordered_set>

namespace chromeos {

namespace secure_channel {

// Pair of IDs belonging to two devices associated with a connection attempt:
// one for the remote device (i.e., the one to which this device is connecting),
// and one for the local device (i.e., the Chromebook).
class DeviceIdPair {
 public:
  DeviceIdPair(const std::string& remote_device_id,
               const std::string& local_device_id);
  DeviceIdPair(const DeviceIdPair& other);
  virtual ~DeviceIdPair();

  const std::string& remote_device_id() const { return remote_device_id_; }
  const std::string& local_device_id() const { return local_device_id_; }

  bool operator==(const DeviceIdPair& other) const;
  bool operator!=(const DeviceIdPair& other) const;
  bool operator<(const DeviceIdPair& other) const;

 private:
  std::string remote_device_id_;
  std::string local_device_id_;
};

struct DeviceIdPairHash {
  size_t operator()(const DeviceIdPair& device_id_pair) const;
};

typedef std::unordered_set<DeviceIdPair, DeviceIdPairHash> DeviceIdPairSet;

std::ostream& operator<<(std::ostream& stream, const DeviceIdPair& details);

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::DeviceIdPair;
using ::chromeos::secure_channel::DeviceIdPairSet;
}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_
