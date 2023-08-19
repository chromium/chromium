// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_error_or.h"

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(FileErrorOrDeathTest, Error) {
  FileErrorOr<int> error = unexpected(File::Error::FILE_ERROR_FAILED);
  EXPECT_THAT(error, test::ErrorIs(File::Error::FILE_ERROR_FAILED));
  EXPECT_DEATH_IF_SUPPORTED(error.value(), "");
}

TEST(FileErrorOrDeathTest, Value) {
  FileErrorOr<int> value = 42;
  EXPECT_THAT(value, test::ValueIs(42));
  EXPECT_DEATH_IF_SUPPORTED(value.error(), "");
}

TEST(FileErrorOrDeathTest, ConstValue) {
  const FileErrorOr<int> const_value = 1234;
  EXPECT_THAT(const_value, test::ValueIs(1234));
  EXPECT_DEATH_IF_SUPPORTED(const_value.error(), "");
}

}  // namespace
}  // namespace base
