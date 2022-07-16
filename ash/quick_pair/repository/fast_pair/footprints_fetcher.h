// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace nearby {
namespace fastpair {
class UserReadDevicesResponse;
}  // namespace fastpair
}  // namespace nearby

namespace ash {
namespace quick_pair {

class HttpFetcher;

using UserReadDevicesCallback = base::OnceCallback<void(
    absl::optional<nearby::fastpair::UserReadDevicesResponse>)>;
using AddDeviceCallback = base::OnceCallback<void(bool)>;
using DeleteDeviceCallback = base::OnceCallback<void(bool)>;

class FootprintsFetcher {
 public:
  FootprintsFetcher();

  FootprintsFetcher(const FootprintsFetcher&) = delete;
  FootprintsFetcher& operator=(const FootprintsFetcher&) = delete;
  virtual ~FootprintsFetcher();

  void GetUserDevices(UserReadDevicesCallback callback);
  void AddUserDevice(nearby::fastpair::FastPairInfo info,
                     AddDeviceCallback callback);
  void DeleteUserDevice(const std::string& hex_account_key,
                        DeleteDeviceCallback callback);

 private:
  void OnGetComplete(UserReadDevicesCallback callback,
                     std::unique_ptr<HttpFetcher> http_fetcher,
                     std::unique_ptr<std::string> response_body);

  void OnPostComplete(AddDeviceCallback callback,
                      std::unique_ptr<HttpFetcher> http_fetcher,
                      std::unique_ptr<std::string> response_body);

  void OnDeleteComplete(DeleteDeviceCallback callback,
                        std::unique_ptr<HttpFetcher> http_fetcher,
                        std::unique_ptr<std::string> response_body);

  base::WeakPtrFactory<FootprintsFetcher> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_
