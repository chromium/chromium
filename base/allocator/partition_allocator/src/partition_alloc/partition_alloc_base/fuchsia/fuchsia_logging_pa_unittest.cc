// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/logger/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/fuchsia/fuchsia_logging.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base {

namespace {

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

}  // namespace

// Verifies the Fuchsia-specific PA_ZX_*() logging macros.
TEST(FuchsiaLoggingTestPA, FuchsiaLogging) {
  MockLogSource mock_log_source;
  constexpr int kTimes =
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      2;
#else
      1;
#endif
  EXPECT_CALL(mock_log_source, Log())
      .Times(kTimes)
      .WillRepeatedly(testing::Return("log message"));

  logging::SetMinLogLevel(logging::LOGGING_INFO);

  EXPECT_TRUE(PA_LOG_IS_ON(INFO));
  EXPECT_EQ(PA_BUILDFLAG(DCHECKS_ARE_ON), PA_DLOG_IS_ON(INFO));

  PA_ZX_LOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();
  PA_ZX_DLOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();

  PA_ZX_CHECK(true, ZX_ERR_INTERNAL);
  PA_ZX_DCHECK(true, ZX_ERR_INTERNAL);
}

}  // namespace partition_alloc::internal::base
