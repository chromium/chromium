// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/rand_util.h"

#include <zircon/syscalls.h>

#include <atomic>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace base {

namespace internal {

namespace {

// The BoringSSl helpers are duplicated in rand_util_posix.cc and
// rand_util_win.cc.
std::atomic<bool> g_use_boringssl;

BASE_FEATURE(kUseBoringSSLForRandBytes,
             "UseBoringSSLForRandBytes",
             FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

void ConfigureBoringSSLBackedRandBytesFieldTrial() {
  g_use_boringssl.store(FeatureList::IsEnabled(kUseBoringSSLForRandBytes),
                        std::memory_order_relaxed);
}

bool UseBoringSSLForRandBytes() {
  return g_use_boringssl.load(std::memory_order_relaxed);
}

}  // namespace internal

void RandBytes(span<uint8_t> output) {
  if (internal::UseBoringSSLForRandBytes()) {
    // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
    (void)RAND_bytes(output.data(), output.size());
    return;
  }

  zx_cprng_draw(output.data(), output.size());
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
