// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/environment_config.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

// This test summarizes which platforms use background thread priority.
TEST(ThreadPoolEnvironmentConfig, CanUseBackgroundPriorityForWorker) {
#if defined(OS_WIN) || defined(OS_APPLE)
  EXPECT_TRUE(CanUseBackgroundPriorityForWorkerThread());
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_FUCHSIA) || \
    defined(OS_CHROMEOS) || defined(OS_NACL)
  EXPECT_FALSE(CanUseBackgroundPriorityForWorkerThread());
#else
#error Platform doesn't match any block
#endif
}

}  // namespace internal
}  // namespace base
