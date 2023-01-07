// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/mem_buffer_util.h"

#include <fuchsia/mem/cpp/fidl.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MemBufferUtilTest, WriteReadVmo) {
  std::string data = "fuchsia";
  zx::vmo vmo = base::VmoFromString(data, "test");
  auto read_data = base::StringFromVmo(vmo);
  ASSERT_TRUE(read_data);
  EXPECT_EQ(*read_data, "fuchsia");
}

TEST(MemBufferUtilTest, WriteReadBuffer) {
  std::string data = "fuchsia";
  fuchsia::mem::Buffer vmo = base::MemBufferFromString(data, "test");
  auto read_data = base::StringFromMemBuffer(vmo);
  ASSERT_TRUE(read_data);
  EXPECT_EQ(*read_data, "fuchsia");
}

}  // namespace base
