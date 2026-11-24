// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_DUMP_WITHOUT_CRASHING_RUST_SHIM_H_
#define BASE_DEBUG_DUMP_WITHOUT_CRASHING_RUST_SHIM_H_

#include "base/location.h"

namespace base::debug {

// Calls `DumpWithoutCrashing()` with the given Rust `Location` (see
// base/location.rs), converted with `Location::CreateForRust()`.
bool DumpWithoutCrashingShim(const RustLocation& location);

}  // namespace base::debug

#endif  // BASE_DEBUG_DUMP_WITHOUT_CRASHING_RUST_SHIM_H_
