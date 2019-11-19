// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/event_name_filter.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/thread_instruction_count.h"
#include "base/trace_event/trace_event_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

const TraceEvent& MakeTraceEvent(const char* name) {
  static TraceEvent event;
  event.Reset(0, TimeTicks(), ThreadTicks(), ThreadInstructionCount(), 'b',
              nullptr, name, "", 0, 0, nullptr, 0);
  return event;
}

TEST(TraceEventNameFilterTest, Whitelist) {
  auto empty_whitelist =
      std::make_unique<EventNameFilter::EventNamesWhitelist>();
  auto filter = std::make_unique<EventNameFilter>(std::move(empty_whitelist));

  // No events should be filtered if the whitelist is empty.
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("foo")));

  auto whitelist = std::make_unique<EventNameFilter::EventNamesWhitelist>();
  whitelist->insert("foo");
  whitelist->insert("bar");
  filter = std::make_unique<EventNameFilter>(std::move(whitelist));
  EXPECT_TRUE(filter->FilterTraceEvent(MakeTraceEvent("foo")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("fooz")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("afoo")));
  EXPECT_TRUE(filter->FilterTraceEvent(MakeTraceEvent("bar")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("foobar")));
}

}  // namespace trace_event
}  // namespace base
