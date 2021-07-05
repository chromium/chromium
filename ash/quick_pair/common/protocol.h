// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_PROTOCOL_H_
#define ASH_QUICK_PAIR_COMMON_PROTOCOL_H_

#include <ostream>

namespace ash {
namespace quick_pair {

enum class Protocol {
  // Google Fast Pair
  kFastPair = 0
};

std::ostream& operator<<(std::ostream& stream, Protocol protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_PROTOCOL_H_
