// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/crc32.h"
#include "third_party/zlib/zlib.h"

namespace base {

uint32_t Crc32(uint32_t sum, span<const uint8_t> data) {
  if (data.empty()) {
    return sum;
  }

  // Make sure zlib checks CPU features before further calls to crc32_z.
  // zlib's crc32_z implementation says it's a convention to call
  // crc32(0, NULL, 0); before making calls to crc32(), so it uses it as
  // a place to cache CPU features if needed.
  // There's no need to cache the results, since there's an internal zlib
  // caching mechanism, so this function will just return if called multiple
  // times.
  (void)crc32_z(0L, Z_NULL, 0);

  return static_cast<uint32_t>(
      (~crc32_z((~sum) & 0xffffffff, data.data(), data.size())) & 0xffffffff);
}

}  // namespace base
