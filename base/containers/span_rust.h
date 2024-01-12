// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_RUST_H_
#define BASE_CONTAINERS_SPAN_RUST_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/rust_buildflags.h"
#include "third_party/rust/cxx/v1/cxx.h"

#if !BUILDFLAG(BUILD_RUST_BASE_CONVERSIONS)
#error "span_rust.h included without BUILD_RUST_BASE_CONVERSIONS"
#endif

namespace base {

// Create a Rust slice from a base::span.
inline rust::Slice<const uint8_t> SpanToRustSlice(span<const uint8_t> span) {
  return rust::Slice<const uint8_t>(span.data(), span.size());
}

// Note to future editors: if you add code to convert from a rust::Slice to
// a base::span, you should be aware that Rust slices (and cxx)
// return a fabricated non-null pointer for zero-length slices, and that
// this will likely need converting to NULL before constructing a span.
// cxx handles the conversion from NULL to an artificial pointer
// (specifically it uses alignof<T>) in the C++ -> Rust direction, but
// does nothing cunning in the other direction, presumably on the
// assumption that C++ won't access the data pointer if the length is 0.
// But we do not think (reinterpret_cast<T*>(alignof(T)), 0) can safely
// be stored in base::span today. The pointer arithmetic
// rules have special cases for NULL, but otherwise it is only defined if
// there is actually an object at the address, and there is no object at
// alignof(T).
// https://eel.is/c++draft/expr.add#4
// https://eel.is/c++draft/expr.add#5

}  // namespace base

#endif  // BASE_CONTAINERS_SPAN_RUST_H_
