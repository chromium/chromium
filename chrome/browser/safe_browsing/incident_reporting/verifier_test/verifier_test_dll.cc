// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some pointless code that will become a DLL with some exports and relocs.

#include <windows.h>

#include <intrin.h>

namespace {

void (*volatile g_somestate)() = nullptr;

}  // namespace

extern "C" void DummyExport() {
  // Emit 256 bytes of nops because the test modifies up to 256 bytes of code.
  // Use nops instead of volatile stores to avoid relocation entries in this
  // region. One of the tests measures the number of modified bytes between
  // relocations, and extra relocations will cause the test to fail.
  // http://crbug.com/636157
  // http://crbug.com/645544
#define T4(x) x; x; x; x
#define NOP4 T4(__nop());
#define NOP16 T4(NOP4);
#define NOP64 T4(NOP16);
#define NOP256 T4(NOP64);
  NOP256;
  g_somestate = nullptr;
}

extern "C"
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    g_somestate = &DummyExport;
  else if (reason == DLL_PROCESS_DETACH)
    g_somestate = nullptr;

  return TRUE;
}
