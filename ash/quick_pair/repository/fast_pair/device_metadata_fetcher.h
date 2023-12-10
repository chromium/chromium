// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_FETCHER_H_

#include <optional>

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace nearby {
namespace fastpair {
class GetObservedDeviceResponse;
}  // namespace fastpair
}  // namespace nearby

namespace ash {
namespace quick_pair {

class HttpFetcher;

using GetObservedDeviceCallback = base::OnceCallback<
    void(std::optional<nearby::fastpair::GetObservedDeviceResponse>, bool)>;

class DeviceMetadataFetcher {
 public:
  DeviceMetadataFetcher();

  // For testing.
  explicit DeviceMetadataFetcher(std::unique_ptr<HttpFetcher> http_fetcher);

  DeviceMetadataFetcher(const DeviceMetadataFetcher&) = delete;
  DeviceMetadataFetcher& operator=(const DeviceMetadataFetcher&) = delete;
  virtual ~DeviceMetadataFetcher();

  void LookupDeviceId(int id, GetObservedDeviceCallback callback);
  void LookupHexDeviceId(const std::string& hex_id,
                         GetObservedDeviceCallback callback);

 private:
  void OnFetchComplete(GetObservedDeviceCallback callback,
                       std::unique_ptr<std::string> response_body,
                       std::unique_ptr<FastPairHttpResult> http_result);
  void OnJsonParsed(GetObservedDeviceCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);

  std::unique_ptr<HttpFetcher> http_fetcher_;

  base::WeakPtrFactory<DeviceMetadataFetcher> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_FETCHER_H_
