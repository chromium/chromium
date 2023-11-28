// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_

#include <optional>

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace nearby {
namespace fastpair {
class UserReadDevicesResponse;
}  // namespace fastpair
}  // namespace nearby

namespace ash {
namespace quick_pair {

using UserReadDevicesCallback = base::OnceCallback<void(
    std::optional<nearby::fastpair::UserReadDevicesResponse>)>;
using AddDeviceCallback = base::OnceCallback<void(bool)>;
using DeleteDeviceCallback = base::OnceCallback<void(bool)>;

class FootprintsFetcher {
 public:
  FootprintsFetcher() = default;

  FootprintsFetcher(const FootprintsFetcher&) = delete;
  FootprintsFetcher& operator=(const FootprintsFetcher&) = delete;
  virtual ~FootprintsFetcher() = default;

  virtual void GetUserDevices(UserReadDevicesCallback callback) = 0;
  virtual void AddUserFastPairInfo(nearby::fastpair::FastPairInfo info,
                                   AddDeviceCallback callback) = 0;
  virtual void DeleteUserDevice(const std::string& hex_account_key,
                                DeleteDeviceCallback callback) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_H_
