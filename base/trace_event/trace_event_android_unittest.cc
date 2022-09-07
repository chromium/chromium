// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(TraceEventAndroidTest, WriteToATrace) {
  // Just a smoke test to ensure no crash.
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->StartATrace("test");
  TRACE_EVENT0("test", "test-event");
  trace_log->StopATrace();
  trace_log->AddClockSyncMetadataEvent();
}

TEST(TraceEventAndroidTest, ATraceStartup) {
  TraceLog* trace_log = TraceLog::GetInstance();
  EXPECT_FALSE(trace_log->TakeATraceStartupConfig());
  trace_log->SetupATraceStartupTrace("cat");
  auto config = trace_log->TakeATraceStartupConfig();
  EXPECT_TRUE(config);
  EXPECT_TRUE(config->IsCategoryGroupEnabled("cat"));
}

}  // namespace trace_event
}  // namespace base
