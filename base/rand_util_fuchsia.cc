// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <zircon/syscalls.h>

#include <atomic>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace base {



void RandBytes(span<uint8_t> output) {
  // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
  (void)RAND_bytes(output.data(), output.size());
}

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  zx_cprng_draw(&number, sizeof(number));
  // This transformation is explained in rand_util.cc.
  return (number >> 11) * 0x1.0p-53;
}

}  // namespace internal

}  // namespace base
