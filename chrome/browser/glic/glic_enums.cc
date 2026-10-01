// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enums.h"

#include <ostream>

namespace glic {

std::ostream& operator<<(std::ostream& os, ClientLoadState state) {
  switch (state) {
    case ClientLoadState::kLoading:
      return os << "kLoading";
    case ClientLoadState::kReady:
      return os << "kReady";
    case ClientLoadState::kError:
      return os << "kError";
  }
  return os << "ClientLoadState(" << static_cast<int>(state) << ")";
}

}  // namespace glic
