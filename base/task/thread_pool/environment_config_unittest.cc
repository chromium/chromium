// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/environment_config.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

// This test summarizes which platforms use background thread priority.
TEST(ThreadPoolEnvironmentConfig, CanUseBackgroundPriorityForWorker) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  EXPECT_TRUE(CanUseBackgroundThreadTypeForWorkerThread());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_NACL)
  EXPECT_FALSE(CanUseBackgroundThreadTypeForWorkerThread());
#else
#error Platform doesn't match any block
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(CanUseUtilityThreadTypeForWorkerThread());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_NACL)
  EXPECT_FALSE(CanUseUtilityThreadTypeForWorkerThread());
#else
#error Platform doesn't match any block
#endif
}

}  // namespace internal
}  // namespace base
