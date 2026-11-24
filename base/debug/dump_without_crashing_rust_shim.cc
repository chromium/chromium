// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/dump_without_crashing_rust_shim.h"

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"

namespace base::debug {

// NOINLINE so that crash reports from Rust callers consistently have this
// frame between the Rust code and DumpWithoutCrashing().
NOINLINE bool DumpWithoutCrashingShim(const RustLocation& location) {
  return DumpWithoutCrashing(Location::CreateForRust(location));
}

}  // namespace base::debug
