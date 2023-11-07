// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_tls.h"

#include <windows.h>

namespace partition_alloc::internal {

namespace {

// Store the key as the thread destruction callback doesn't get it.
PartitionTlsKey g_key;
void (*g_destructor)(void*) = nullptr;
void (*g_on_dll_process_detach)() = nullptr;

// Static callback function to call with each thread termination.
void NTAPI PartitionTlsOnThreadExit(PVOID module,
                                    DWORD reason,
                                    PVOID reserved) {
  if (reason != DLL_THREAD_DETACH && reason != DLL_PROCESS_DETACH) {
    return;
  }

  if (reason == DLL_PROCESS_DETACH && g_on_dll_process_detach) {
    g_on_dll_process_detach();
  }

  if (g_destructor) {
    void* per_thread_data = PartitionTlsGet(g_key);
    if (per_thread_data) {
      g_destructor(per_thread_data);
    }
  }
}

}  // namespace

bool PartitionTlsCreate(PartitionTlsKey* key, void (*destructor)(void*)) {
  PA_CHECK(g_destructor == nullptr);  // Only one TLS key supported at a time.
  PartitionTlsKey value = TlsAlloc();
  if (value != TLS_OUT_OF_INDEXES) {
    *key = value;

    g_key = value;
    g_destructor = destructor;
    return true;
  }
  return false;
}

void PartitionTlsSetOnDllProcessDetach(void (*callback)()) {
  g_on_dll_process_detach = callback;
}

}  // namespace partition_alloc::internal

// See thread_local_storage_win.cc for details and reference.
//
// The callback has to be in any section between .CRT$XLA and .CRT$XLZ, as these
// are sentinels used by the TLS code to find the callback array bounds. As we
// don't particularly care about where we are called but would prefer to be
// deinitialized towards the end (in particular after Chromium's TLS), we locate
// ourselves in .CRT$XLY.

// Force a reference to _tls_used to make the linker create the TLS directory if
// it's not already there.  (e.g. if __declspec(thread) is not used).  Force a
// reference to partition_tls_thread_exit_callback to prevent whole program
// optimization from discarding the variable.
#ifdef _WIN64

#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:partition_tls_thread_exit_callback")

#else  // _WIN64

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_partition_tls_thread_exit_callback")

#endif  // _WIN64

// extern "C" suppresses C++ name mangling so we know the symbol name for the
// linker /INCLUDE:symbol pragma above.
extern "C" {
// The linker must not discard partition_tls_thread_exit_callback.  (We force a
// reference to this variable with a linker /INCLUDE:symbol pragma to ensure
// that.) If this variable is discarded, PartitionTlsOnThreadExit will never be
// called.
#ifdef _WIN64

// .CRT section is merged with .rdata on x64 so it must be constant data.
#pragma const_seg(".CRT$XLY")
// When defining a const variable, it must have external linkage to be sure the
// linker doesn't discard it.
extern const PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback;
const PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback =
    partition_alloc::internal::PartitionTlsOnThreadExit;

// Reset the default section.
#pragma const_seg()

#else  // _WIN64

#pragma data_seg(".CRT$XLY")
PIMAGE_TLS_CALLBACK partition_tls_thread_exit_callback =
    partition_alloc::internal::PartitionTlsOnThreadExit;

// Reset the default section.
#pragma data_seg()

#endif  // _WIN64
}       // extern "C"
