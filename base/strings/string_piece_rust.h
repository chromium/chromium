// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_PIECE_RUST_H_
#define BASE_STRINGS_STRING_PIECE_RUST_H_

#include <stdint.h>

#include "base/rust_buildflags.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/rust/cxx/v1/crate/include/cxx.h"

#if !BUILDFLAG(BUILD_RUST_BASE_CONVERSIONS)
#error "string_piece_rust.h included without BUILD_RUST_BASE_CONVERSIONS"
#endif

namespace base {

// Create a Rust str from a base::BasicStringPiece. This will call std::abort
// if there is any invalid UTF8. If you're concerned about this, then
// instead use StringPieceToRustSlice and convert the data to a string on
// the Rust side (or pass in a std::string).
inline rust::Str StringPieceToRustStrUTF8(StringPiece string_piece) {
  return rust::Str(string_piece.data(), string_piece.size());
}

// Create a Rust slice from a StringPiece. No UTF8 check is performed.
inline rust::Slice<const uint8_t> StringPieceToRustSlice(
    StringPiece string_piece) {
  return rust::Slice<const uint8_t>(
      reinterpret_cast<const uint8_t*>(string_piece.data()),
      string_piece.length() * sizeof(StringPiece::value_type));
}

// Create a StringPiece from a Rust str.
inline StringPiece RustStrToStringPiece(rust::Str str) {
  return StringPiece(str.data(), str.size());
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_RUST_H_
