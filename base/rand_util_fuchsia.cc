// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <zircon/syscalls.h>

#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace base {

void RandBytes(void* output, size_t output_length) {
  // Ensure BoringSSL is initialized so it can use things like RDRAND.
  CRYPTO_library_init();
  // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
  (void)RAND_bytes(static_cast<uint8_t*>(output), output_length);
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
