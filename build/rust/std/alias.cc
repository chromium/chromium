// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been copied from //base/debug/alias.cc ( additionally the APIs
// were moved into the `build_rust_std` namespace).
//
// TODO(crbug.com/40279749): Avoid code duplication / reuse code.

#include "build/rust/std/alias.h"

#include "build/rust/std/compiler_specific.h"

namespace build_rust_std {
namespace debug {

// This file/function should be excluded from LTO/LTCG to ensure that the
// compiler can't see this function's implementation when compiling calls to it.
NOINLINE void Alias(const void* var) {}

}  // namespace debug
}  // namespace build_rust_std
