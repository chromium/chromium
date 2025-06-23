// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/metrics_hashes.h"

#include <string.h>

#include <array>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "third_party/boringssl/src/include/openssl/md5.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace base {
uint64_t HashMetricName(std::string_view name) {
  // Corresponding Python code for quick look up:
  //
  //   import struct
  //   import hashlib
  //   struct.unpack('>Q', hashlib.md5(name.encode('utf-8')).digest()[:8])[0]
  //
  std::array<uint8_t, MD5_DIGEST_LENGTH> hash;
  ::MD5(reinterpret_cast<const uint8_t*>(name.data()), name.size(),
        hash.data());
  return U64FromBigEndian(base::span(hash).first<8>());
}

uint32_t HashMetricNameAs32Bits(std::string_view name) {
  std::array<uint8_t, MD5_DIGEST_LENGTH> hash;
  ::MD5(reinterpret_cast<const uint8_t*>(name.data()), name.size(),
        hash.data());
  return U32FromBigEndian(base::span(hash).first<4>());
}

uint32_t ParseMetricHashTo32Bits(uint64_t hash) {
  return (hash >> 32);
}

uint32_t HashFieldTrialName(std::string_view name) {
  // SHA-1 is designed to produce a uniformly random spread in its output space,
  // even for nearly-identical inputs.
  std::array<uint8_t, SHA_DIGEST_LENGTH> hash;
  ::SHA1(reinterpret_cast<const uint8_t*>(name.data()), name.size(),
         hash.data());
  return U32FromLittleEndian(base::span(hash).first<4>());
}

}  // namespace base
