// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_count.h"

#include <iomanip>
#include <ostream>

namespace base {

std::ostream& operator<<(std::ostream& os, ByteCount byte_count) {
  // Save the original stream state
  std::ios_base::fmtflags original_flags = os.flags();
  std::streamsize original_precision = os.precision();

  const int64_t bytes = byte_count.InBytes();
  if (bytes % GiB(1).InBytes() == 0) {
    os << byte_count.InGiB() << "GiB";
  } else if (bytes % MiB(1).InBytes() == 0) {
    os << byte_count.InMiB() << "MiB";
  } else if (bytes % KiB(1).InBytes() == 0) {
    os << byte_count.InKiB() << "KiB";
  } else {
    os << bytes << "B";
    if (bytes > KiB(1).InBytes()) {
      os << " (" << std::fixed << std::setprecision(3);
      if (bytes > GiB(1).InBytes()) {
        os << byte_count.InGiBF() << "GiB";
      } else if (bytes > MiB(1).InBytes()) {
        os << byte_count.InMiBF() << "MiB";
      } else {
        os << byte_count.InKiBF() << "KiB";
      }
      os << ")";
    }
  }

  // Restore the original stream state before returning
  os.flags(original_flags);
  os.precision(original_precision);

  return os;
}

}  // namespace base
