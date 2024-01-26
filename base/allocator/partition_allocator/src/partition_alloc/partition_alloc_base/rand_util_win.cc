// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/rand_util.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "partition_alloc/partition_alloc_base/check.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace partition_alloc::internal::base {

void RandBytes(void* output, size_t output_length) {
  // Import bcryptprimitives directly rather than cryptbase to avoid opening a
  // handle to \\Device\KsecDD in the renderer.
  // Note: we cannot use a magic static here as PA runs too early in process
  // startup, but this should be safe as the process will be single-threaded
  // when this first runs.
  static decltype(&ProcessPrng) process_prng_fn = nullptr;
  if (!process_prng_fn) {
    HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
    PA_BASE_CHECK(hmod);
    process_prng_fn = reinterpret_cast<decltype(&ProcessPrng)>(
        GetProcAddress(hmod, "ProcessPrng"));
    PA_BASE_CHECK(process_prng_fn);
  }
  BOOL success = process_prng_fn(static_cast<BYTE*>(output), output_length);
  // ProcessPrng is documented to always return TRUE.
  PA_BASE_CHECK(success);
}

}  // namespace partition_alloc::internal::base
