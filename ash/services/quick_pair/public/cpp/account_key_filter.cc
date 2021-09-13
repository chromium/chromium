// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/public/cpp/account_key_filter.h"

#include <math.h>
#include <cstddef>
#include <iterator>

#include "ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "crypto/sha2.h"

namespace ash {
namespace quick_pair {

constexpr int kBitsInByte = 8;

AccountKeyFilter::AccountKeyFilter(
    const NotDiscoverableAdvertisement& advertisement)
    : AccountKeyFilter(advertisement.account_key_filter, advertisement.salt) {}

AccountKeyFilter::AccountKeyFilter(
    const std::vector<uint8_t>& account_key_filter_bytes,
    uint8_t salt)
    : bit_sets_(account_key_filter_bytes), salt_(salt) {}

AccountKeyFilter::~AccountKeyFilter() = default;

bool AccountKeyFilter::Test(const std::vector<uint8_t>& account_key_bytes) {
  if (bit_sets_.empty())
    return false;

  // We first need to append the salt value to the input (see
  // https://developers.google.com/nearby/fast-pair/spec#AccountKeyFilter).
  std::vector<uint8_t> data(account_key_bytes);
  data.push_back(salt_);

  std::array<uint8_t, 32> hashed = crypto::SHA256Hash(data);

  // Iterate over the hashed input in 4 byte increments, combine those 4 bytes
  // into an unsigned int and use it as the index into our |bit_sets_|.
  for (size_t i = 0; i < hashed.size(); i += 4) {
    uint32_t hash = uint32_t{hashed[i]} << 24 | uint32_t{hashed[i + 1]} << 16 |
                    uint32_t{hashed[i + 2]} << 8 | hashed[i + 3];

    size_t num_bits = bit_sets_.size() * kBitsInByte;
    size_t n = hash % num_bits;
    size_t byte_index = floor(n / kBitsInByte);
    size_t bit_index = n % kBitsInByte;
    bool is_set = (bit_sets_[byte_index] >> bit_index) & 0x01;

    if (!is_set)
      return false;
  }

  return true;
}

}  // namespace quick_pair
}  // namespace ash
