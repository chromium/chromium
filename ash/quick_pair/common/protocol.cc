// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/protocol.h"

namespace ash {
namespace quick_pair {

std::ostream& operator<<(std::ostream& stream, Protocol protocol) {
  switch (protocol) {
    case Protocol::kFastPair:
      stream << "[Fast Pair]";
      break;
  }

  return stream;
}

}  // namespace quick_pair
}  // namespace ash
