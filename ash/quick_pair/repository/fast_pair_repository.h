// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothDevice;
}

namespace ash {
namespace quick_pair {

class DeviceMetadataFetcher;
class FastPairImageDecoder;

using DeviceMetadataCallback = base::OnceCallback<void(DeviceMetadata*)>;

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepository {
 public:
  static FastPairRepository* Get();

  FastPairRepository();
  FastPairRepository(const FastPairRepository&) = delete;
  FastPairRepository& operator=(const FastPairRepository&) = delete;
  virtual ~FastPairRepository();

  // Returns the DeviceMetadata for a given |hex_model_id| to the provided
  // |callback|, if available.
  void GetDeviceMetadata(const std::string& hex_model_id,
                         DeviceMetadataCallback callback);

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

 private:
  static void SetInstance(FastPairRepository* instance);

  void OnMetadataFetched(
      const std::string& hex_model_id,
      DeviceMetadataCallback callback,
      absl::optional<nearby::fastpair::GetObservedDeviceResponse> response);
  void OnImageDecoded(const std::string& hex_model_id,
                      DeviceMetadataCallback callback,
                      nearby::fastpair::GetObservedDeviceResponse response,
                      gfx::Image image);

  std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher_;
  std::unique_ptr<FastPairImageDecoder> image_decoder_;
  base::flat_map<std::string, std::unique_ptr<DeviceMetadata>> metadata_cache_;
  base::WeakPtrFactory<FastPairRepository> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
