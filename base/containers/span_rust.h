// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_RUST_H_
#define BASE_CONTAINERS_SPAN_RUST_H_

#include "build/rust/rust_buildflags.h"

#if BUILDFLAG(TOOLCHAIN_HAS_RUST)

#include <stdint.h>

#include "base/containers/span.h"
#include "third_party/rust/cxx/v1/crate/include/cxx.h"

namespace base {

// Create a Rust slice from a base::span.
inline rust::Slice<const uint8_t> SpanToRustSlice(span<const uint8_t> span) {
  return rust::Slice<const uint8_t>(span.data(), span.size());
}

}  // namespace base

#endif  // BUILDFLAG(TOOLCHAIN_HAS_RUST)

#endif  // BASE_CONTAINERS_SPAN_RUST_H_
