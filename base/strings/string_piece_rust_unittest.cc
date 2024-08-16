// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/string_view_rust.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(BaseStringViewRustTest, StrRoundTrip) {
  std::string data = "hello";
  std::string_view data_piece(data);
  rust::Str rust_str = StringViewToRustStrUTF8(data_piece);
  EXPECT_EQ(5ul, rust_str.length());
  std::string_view data_piece2 = RustStrToStringView(rust_str);
  EXPECT_EQ(data_piece, data_piece2);
}

TEST(BaseStringViewRustTest, StrToSlice) {
  std::string data = "hello";
  std::string_view data_piece(data);
  rust::Slice<const uint8_t> rust_slice = StringViewToRustSlice(data_piece);
  EXPECT_EQ(5ul, rust_slice.length());
  EXPECT_EQ('e', rust_slice[1]);
}

}  // namespace
}  // namespace base
