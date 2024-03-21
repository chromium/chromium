// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/metrics_hashes.h"

#include <string.h>

#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/hash/sha1.h"
#include "base/numerics/byte_conversions.h"

namespace base {
namespace {

// Converts the 8-byte prefix of an MD5 hash into a uint64_t value.
inline uint64_t DigestToUInt64(const base::MD5Digest& digest) {
  return base::numerics::U64FromBigEndian(base::span(digest.a).first<8u>());
}

// Converts the 4-byte prefix of an MD5 hash into a uint32_t value.
inline uint32_t DigestToUInt32(const base::MD5Digest& digest) {
  return base::numerics::U32FromBigEndian(base::span(digest.a).first<4u>());
}

}  // namespace

uint64_t HashMetricName(std::string_view name) {
  // Corresponding Python code for quick look up:
  //
  //   import struct
  //   import hashlib
  //   struct.unpack('>Q', hashlib.md5(name.encode('utf-8')).digest()[:8])[0]
  //
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(name), &digest);
  return DigestToUInt64(digest);
}

uint32_t HashMetricNameAs32Bits(std::string_view name) {
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(name), &digest);
  return DigestToUInt32(digest);
}

uint32_t HashFieldTrialName(std::string_view name) {
  // SHA-1 is designed to produce a uniformly random spread in its output space,
  // even for nearly-identical inputs.
  SHA1Digest sha1_hash = base::SHA1HashSpan(base::as_byte_span(name));
  return base::numerics::U32FromLittleEndian(base::span(sha1_hash).first<4u>());
}

}  // namespace base
