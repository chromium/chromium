// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_ACCOUNT_KEY_FILTER_H_
#define ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_ACCOUNT_KEY_FILTER_H_

#include <vector>

namespace ash {
namespace quick_pair {

struct NotDiscoverableAdvertisement;

// Class which represents a Fast Pair Account Key Filter (see
// https://developers.google.com/nearby/fast-pair/spec#AccountKeyFilter) and
// exposes a method to test account key membership.
class AccountKeyFilter {
 public:
  explicit AccountKeyFilter(const NotDiscoverableAdvertisement& advertisement);
  AccountKeyFilter(const std::vector<uint8_t>& account_key_filter_bytes,
                   uint8_t salt);
  AccountKeyFilter(const AccountKeyFilter&) = delete;
  AccountKeyFilter& operator=(const AccountKeyFilter&) = delete;
  ~AccountKeyFilter();

  // Tests whether the |account_key_bytes| belong to this Account Key Filter.
  // Note: The return value may be a false positive, but will never be a false
  // negative.
  bool Test(const std::vector<uint8_t>& account_key_bytes);

 private:
  std::vector<uint8_t> bit_sets_;
  uint8_t salt_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_ACCOUNT_KEY_FILTER_H_
