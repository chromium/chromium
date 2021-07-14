// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothDevice;
}

namespace ash {
namespace quick_pair {

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepository {
 public:
  FastPairRepository();
  FastPairRepository(const FastPairRepository&) = delete;
  FastPairRepository& operator=(const FastPairRepository&) = delete;
  virtual ~FastPairRepository();

  // Returns the DeviceMetadata for a given |hex_model_id| to the provided
  // |callback|, if available.
  void GetDeviceMetadata(
      const std::string& hex_model_id,
      base::OnceCallback<void(absl::optional<nearby::fastpair::Device>)>
          callback);

  // Checks if the input |hex_model_id| is valid and notifies the requester
  // through the provided |callback|.
  void IsValidModelId(const std::string& hex_model_id,
                      base::OnceCallback<void(bool)> callback);

  // Looks up the key associated with either |address| or |account_key_filter|
  // and returns it to the provided |callback|, if available.  If this
  // information is available locally that will be returned immediately,
  // otherwise this will request data from the footprints server.
  void GetAssociatedAccountKey(
      const std::string& address,
      const std::string& account_key_filter,
      base::OnceCallback<void(absl::optional<std::string>)> callback);

  // Stores the given |account_key| for a |device| on the server.
  void AssociateAccountKey(const Device& device,
                           const std::string& account_key);

  // Deletes the associated data for a given |device|.
  void DeleteAssociatedDevice(const device::BluetoothDevice* device);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
