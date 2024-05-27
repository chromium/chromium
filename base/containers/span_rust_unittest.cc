// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "base/containers/span_rust.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(BaseSpanRustTest, SliceConstruct) {
  uint8_t data[] = {0, 1, 2, 3, 4};
  span<const uint8_t> data_span(data, 2u);
  rust::Slice<const uint8_t> rust_slice = SpanToRustSlice(data_span);
  EXPECT_EQ(2ul, rust_slice.length());
  EXPECT_EQ(1, rust_slice[1]);
}

}  // namespace
}  // namespace base
