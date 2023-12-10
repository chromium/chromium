// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAKE_FOOTPRINTS_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAKE_FOOTPRINTS_FETCHER_H_

#include <optional>

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/footprints_fetcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace quick_pair {

class FakeFootprintsFetcher : public FootprintsFetcher {
 public:
  FakeFootprintsFetcher();
  FakeFootprintsFetcher(const FakeFootprintsFetcher&) = delete;
  FakeFootprintsFetcher& operator=(const FakeFootprintsFetcher&) = delete;
  ~FakeFootprintsFetcher() override;

  // FootprintsFetcher::
  void GetUserDevices(UserReadDevicesCallback callback) override;
  void AddUserFastPairInfo(nearby::fastpair::FastPairInfo info,
                           AddDeviceCallback callback) override;
  void DeleteUserDevice(const std::string& hex_account_key,
                        DeleteDeviceCallback callback) override;

  bool ContainsKey(const std::vector<uint8_t>& account_key);

  void SetGetUserDevicesResponse(
      std::optional<nearby::fastpair::UserReadDevicesResponse> response);

  void SetAddUserFastPairInfoResult(bool add_user_result);

  void SetDeleteUserDeviceResult(bool delete_device_result);

 private:
  bool add_user_result_ = true;
  bool delete_device_result_ = true;
  bool response_set_ = false;
  std::optional<nearby::fastpair::UserReadDevicesResponse> response_;
  nearby::fastpair::FastPairInfo opt_in_status_info_;
  base::flat_map<std::string, nearby::fastpair::FastPairInfo>
      account_key_to_info_map_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAKE_FOOTPRINTS_FETCHER_H_
