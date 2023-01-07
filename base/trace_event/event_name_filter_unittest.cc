// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/event_name_filter.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

const TraceEvent& MakeTraceEvent(const char* name) {
  static TraceEvent event;
  event.Reset(0, TimeTicks(), ThreadTicks(), 'b', nullptr, name, "", 0, 0,
              nullptr, 0);
  return event;
}

TEST(TraceEventNameFilterTest, Allowlist) {
  auto empty_allowlist =
      std::make_unique<EventNameFilter::EventNamesAllowlist>();
  auto filter = std::make_unique<EventNameFilter>(std::move(empty_allowlist));

  // No events should be filtered if the allowlist is empty.
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("foo")));

  auto allowlist = std::make_unique<EventNameFilter::EventNamesAllowlist>();
  allowlist->insert("foo");
  allowlist->insert("bar");
  filter = std::make_unique<EventNameFilter>(std::move(allowlist));
  EXPECT_TRUE(filter->FilterTraceEvent(MakeTraceEvent("foo")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("fooz")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("afoo")));
  EXPECT_TRUE(filter->FilterTraceEvent(MakeTraceEvent("bar")));
  EXPECT_FALSE(filter->FilterTraceEvent(MakeTraceEvent("foobar")));
}

}  // namespace trace_event
}  // namespace base
