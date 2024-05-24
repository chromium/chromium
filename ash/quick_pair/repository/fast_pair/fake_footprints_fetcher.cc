// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/fake_footprints_fetcher.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "base/strings/string_number_conversions.h"

namespace ash {
namespace quick_pair {

FakeFootprintsFetcher::FakeFootprintsFetcher() = default;
FakeFootprintsFetcher::~FakeFootprintsFetcher() = default;

void FakeFootprintsFetcher::GetUserDevices(UserReadDevicesCallback callback) {
  if (response_set_) {
    std::move(callback).Run(std::move(response_));
    return;
  }

  nearby::fastpair::UserReadDevicesResponse response;
  for (const auto& entry : account_key_to_info_map_) {
    *response.add_fast_pair_info() = entry.second;
  }

  if (add_user_result_) {
    *response.add_fast_pair_info() = opt_in_status_info_;
  }
  std::move(callback).Run(std::move(response));
}

void FakeFootprintsFetcher::SetGetUserDevicesResponse(
    std::optional<nearby::fastpair::UserReadDevicesResponse> response) {
  response_set_ = true;
  response_ = response;
}

void FakeFootprintsFetcher::SetAddUserFastPairInfoResult(bool add_user_result) {
  add_user_result_ = add_user_result;
}

void FakeFootprintsFetcher::AddUserFastPairInfo(
    nearby::fastpair::FastPairInfo info,
    AddDeviceCallback callback) {
  if (!add_user_result_) {
    std::move(callback).Run(add_user_result_);
    return;
  }

  account_key_to_info_map_[base::HexEncode(
      base::as_byte_span(info.device().account_key()))] = info;
  std::move(callback).Run(add_user_result_);
}

void FakeFootprintsFetcher::SetDeleteUserDeviceResult(
    bool delete_device_result) {
  delete_device_result_ = delete_device_result;
}

void FakeFootprintsFetcher::DeleteUserDevice(const std::string& hex_account_key,
                                             DeleteDeviceCallback callback) {
  if (!delete_device_result_) {
    std::move(callback).Run(false);
    return;
  }

  account_key_to_info_map_.erase(hex_account_key);
  std::move(callback).Run(true);
}

bool FakeFootprintsFetcher::ContainsKey(
    const std::vector<uint8_t>& account_key) {
  return account_key_to_info_map_.contains(base::HexEncode(account_key));
}

}  // namespace quick_pair
}  // namespace ash
