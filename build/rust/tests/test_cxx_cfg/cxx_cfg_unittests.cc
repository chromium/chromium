// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "build/build_config.h"
#include "build/rust/tests/test_cxx_cfg/cxx_cfg_lib.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(RustCxxCfgTest, MainTest) {
// `#[cfg(target_family = "unix")]` covers Fuchsia, but
// `BUILD_FLAG(IS_POSIX)` does not cover Fuchsia.
//
// So we need to `||` two conditions together to get an equivalent of Rust-side
// `#[cfg(target_family = "unix")]` from `cxx_cfg_lib.rs`.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  uint32_t actual = rust_test::double_unix_value(123);
#else
  uint32_t actual = rust_test::double_non_unix_value(123);
#endif
  EXPECT_EQ(actual, 123u * 2u);
}

}  // namespace
