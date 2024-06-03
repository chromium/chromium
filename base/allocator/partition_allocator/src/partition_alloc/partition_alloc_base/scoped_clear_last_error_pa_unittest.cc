// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/scoped_clear_last_error.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // PA_BUILDFLAG(IS_WIN)

namespace partition_alloc::internal::base {

TEST(PAScopedClearLastError, TestNoError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    EXPECT_EQ(0, errno);
  }
  EXPECT_EQ(1, errno);
}

TEST(PAScopedClearLastError, TestError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    errno = 2;
  }
  EXPECT_EQ(1, errno);
}

#if PA_BUILDFLAG(IS_WIN)

TEST(PAScopedClearLastError, TestNoErrorWin) {
  ::SetLastError(1);
  {
    ScopedClearLastError clear_error;
    EXPECT_EQ(logging::SystemErrorCode(0), ::GetLastError());
  }
  EXPECT_EQ(logging::SystemErrorCode(1), ::GetLastError());
}

TEST(PAScopedClearLastError, TestErrorWin) {
  ::SetLastError(1);
  {
    ScopedClearLastError clear_error;
    ::SetLastError(2);
  }
  EXPECT_EQ(logging::SystemErrorCode(1), ::GetLastError());
}

#endif  // PA_BUILDFLAG(IS_WIN)

}  // namespace partition_alloc::internal::base
