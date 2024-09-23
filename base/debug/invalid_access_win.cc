// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/invalid_access_win.h"

#include <windows.h>

#include <intrin.h>
#include <stdlib.h>

#include "base/check.h"
#include "build/build_config.h"

namespace base {
namespace debug {
namespace win {

namespace {

#if defined(ARCH_CPU_X86_FAMILY)
// On x86/x64 systems, nop instructions are generally 1 byte.
static constexpr int kNopInstructionSize = 1;
#elif defined(ARCH_CPU_ARM64)
// On Arm systems, all instructions are 4 bytes, fixed size.
static constexpr int kNopInstructionSize = 4;
#else
#error "Unsupported architecture"
#endif

// Function that can be jumped midway into safely.
__attribute__((naked)) int nop_sled() {
  asm("nop\n"
      "nop\n"
      "ret\n");
}

using FuncType = decltype(&nop_sled);

void IndirectCall(FuncType* func) {
  // This code always generates CFG guards.
  (*func)();
}

}  // namespace

void TerminateWithHeapCorruption() {
  __try {
    HANDLE heap = ::HeapCreate(0, 0, 0);
    CHECK(heap);
    CHECK(HeapSetInformation(heap, HeapEnableTerminationOnCorruption, nullptr,
                             0));
    void* addr = ::HeapAlloc(heap, 0, 0x1000);
    CHECK(addr);
    // Corrupt heap header.
    char* addr_mutable = reinterpret_cast<char*>(addr);
    memset(addr_mutable - sizeof(addr), 0xCC, sizeof(addr));

    HeapFree(heap, 0, addr);
    HeapDestroy(heap);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Heap corruption exception should never be caught.
    CHECK(false);
  }
  // Should never reach here.
  abort();
}

void TerminateWithControlFlowViolation() {
  // Call into the middle of the NOP sled.
  FuncType func = reinterpret_cast<FuncType>(
      (reinterpret_cast<uintptr_t>(nop_sled)) + kNopInstructionSize);
  __try {
    // Generates a STATUS_STACK_BUFFER_OVERRUN exception if CFG triggers.
    IndirectCall(&func);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // CFG fast fail should never be caught.
    CHECK(false);
  }
  // Should only reach here if CFG is disabled.
  abort();
}

}  // namespace win
}  // namespace debug
}  // namespace base
