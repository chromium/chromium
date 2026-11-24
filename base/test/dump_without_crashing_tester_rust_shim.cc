// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/dump_without_crashing_tester_rust_shim.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"

namespace base::test::rust {

namespace {

int32_t g_dump_count = 0;

void CountDump() {
  ++g_dump_count;
}

}  // namespace

// static
std::unique_ptr<DumpWithoutCrashingTesterRs>
DumpWithoutCrashingTesterRs::Create() {
  return std::make_unique<DumpWithoutCrashingTesterRs>();
}

DumpWithoutCrashingTesterRs::DumpWithoutCrashingTesterRs() {
  g_dump_count = 0;
  debug::ResetDumpWithoutCrashingThrottlingForTesting();
  debug::SetDumpWithoutCrashingFunction(&CountDump);
}

DumpWithoutCrashingTesterRs::~DumpWithoutCrashingTesterRs() {
  debug::SetDumpWithoutCrashingFunction(nullptr);
  debug::ResetDumpWithoutCrashingThrottlingForTesting();
}

int32_t DumpWithoutCrashingTesterRs::DumpCount() const {
  return g_dump_count;
}

}  // namespace base::test::rust
