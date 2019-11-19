// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_pin_type.h"

#include "base/logging.h"

namespace ash {

std::ostream& operator<<(std::ostream& out, WindowPinType pin_type) {
  switch (pin_type) {
    case WindowPinType::kNone:
      return out << "kNone";
    case WindowPinType::kPinned:
      return out << "kPinned";
    case WindowPinType::kTrustedPinned:
      return out << "kTrustedPinned";
  }

  NOTREACHED();
  return out;
}

}  // namespace ash
