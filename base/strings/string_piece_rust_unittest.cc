// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece_rust.h"
#include "build/rust/rust_buildflags.h"

#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(TOOLCHAIN_HAS_RUST)

namespace base {
namespace {

TEST(BaseStringPieceRustTest, StrRoundTrip) {
  std::string data = "hello";
  StringPiece data_piece(data);
  rust::Str rust_str = StringPieceToRustStrUTF8(data_piece);
  EXPECT_EQ(5ul, rust_str.length());
  StringPiece data_piece2 = RustStrToStringPiece(rust_str);
  EXPECT_EQ(data_piece, data_piece2);
}

TEST(BaseStringPieceRustTest, StrToSlice) {
  std::string data = "hello";
  StringPiece data_piece(data);
  rust::Slice<const uint8_t> rust_slice = StringPieceToRustSlice(data_piece);
  EXPECT_EQ(5ul, rust_slice.length());
  EXPECT_EQ('e', rust_slice[1]);
}

}  // namespace
}  // namespace base

#endif  // BUILDFLAG(TOOLCHAIN_HAS_RUST)
