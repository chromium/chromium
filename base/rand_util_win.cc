// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/rand_util.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <limits>

#include "base/check.h"
#include "base/feature_list.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace base {

namespace internal {

namespace {

// The BoringSSl helpers are duplicated in rand_util_fuchsia.cc and
// rand_util_posix.cc.
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

namespace {

// Import bcryptprimitives!ProcessPrng rather than cryptbase!RtlGenRandom to
// avoid opening a handle to \\Device\KsecDD in the renderer.
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  CHECK(hmod);
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  CHECK(process_prng_fn);
  return process_prng_fn;
}

void RandBytesInternal(span<uint8_t> output, bool avoid_allocation) {
  if (!avoid_allocation && internal::UseBoringSSLForRandBytes()) {
    // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
    (void)RAND_bytes(output.data(), output.size());
    return;
  }

  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  BOOL success =
      process_prng_fn(static_cast<BYTE*>(output.data()), output.size());
  // ProcessPrng is documented to always return TRUE.
  CHECK(success);
}

}  // namespace

void RandBytes(span<uint8_t> output) {
  RandBytesInternal(output, /*avoid_allocation=*/false);
}

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  RandBytesInternal(byte_span_from_ref(number),
                    /*avoid_allocation=*/true);
  // This transformation is explained in rand_util.cc.
  return (number >> 11) * 0x1.0p-53;
}

}  // namespace internal

}  // namespace base
