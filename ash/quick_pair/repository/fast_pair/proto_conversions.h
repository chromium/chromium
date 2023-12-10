// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/quick_pair/proto/fastpair.pb.h"

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PROTO_CONVERSIONS_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PROTO_CONVERSIONS_H_

namespace ash {
namespace quick_pair {

class DeviceMetadata;

nearby::fastpair::FastPairInfo BuildFastPairInfo(
    const std::string& hex_model_id,
    const std::vector<uint8_t>& account_key,
    const std::string& mac_address,
    const std::optional<std::string>& display_name,
    DeviceMetadata* metadata);

nearby::fastpair::FastPairInfo BuildFastPairInfoForOptIn(
    nearby::fastpair::OptInStatus opt_in_status);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PROTO_CONVERSIONS_H_
