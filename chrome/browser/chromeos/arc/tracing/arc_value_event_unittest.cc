// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_value_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_value_event_trimmer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcValueEventTest = testing::Test;
using Type = ArcValueEvent::Type;

TEST_F(ArcValueEventTest, Trimmer) {
  ValueEvents events;
  {
    ArcValueEventTrimmer trimmer(&events, ArcValueEvent::Type::kMemUsed);
    trimmer.MaybeAdd(100 /* timestamp */, 1 /* value */);
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(ArcValueEvent(100, ArcValueEvent::Type::kMemUsed, 1), events[0]);
    trimmer.MaybeAdd(101, 2);
    ASSERT_EQ(2U, events.size());
    EXPECT_EQ(ArcValueEvent(101, ArcValueEvent::Type::kMemUsed, 2), events[1]);
    trimmer.MaybeAdd(102, 2);
    EXPECT_EQ(2U, events.size());
    trimmer.MaybeAdd(103, 2);
    EXPECT_EQ(2U, events.size());
    trimmer.MaybeAdd(104, 2);
    EXPECT_EQ(2U, events.size());
    trimmer.MaybeAdd(105, 1);
    ASSERT_EQ(4U, events.size());
    EXPECT_EQ(ArcValueEvent(104, ArcValueEvent::Type::kMemUsed, 2), events[2]);
    EXPECT_EQ(ArcValueEvent(105, ArcValueEvent::Type::kMemUsed, 1), events[3]);
    trimmer.MaybeAdd(106, 2);
    ASSERT_EQ(5U, events.size());
    EXPECT_EQ(ArcValueEvent(106, ArcValueEvent::Type::kMemUsed, 2), events[4]);
    trimmer.MaybeAdd(107, 2);
    EXPECT_EQ(5U, events.size());
  }
  // Check auto-close, last trimmed is added.
  ASSERT_EQ(6U, events.size());
  EXPECT_EQ(ArcValueEvent(107, ArcValueEvent::Type::kMemUsed, 2), events[5]);
}

TEST_F(ArcValueEventTest, TrimmerResetConstant) {
  ValueEvents events;
  {
    ArcValueEventTrimmer trimmer(&events, ArcValueEvent::Type::kMemUsed);
    trimmer.MaybeAdd(100 /* timestamp */, 0 /* value */);
    trimmer.MaybeAdd(200 /* timestamp */, 0 /* value */);
    EXPECT_EQ(1U, events.size());
    trimmer.ResetIfConstant(1 /* value */);
    EXPECT_EQ(1U, events.size());
    trimmer.ResetIfConstant(0 /* value */);
    EXPECT_TRUE(events.empty());
  }
  // Flush does not change latest state.
  EXPECT_TRUE(events.empty());
}

TEST_F(ArcValueEventTest, Serialize) {
  const ValueEvents events{
      {100 /* timestamp */, ArcValueEvent::Type::kMemTotal, 10 /* value */},
      {101 /* timestamp */, ArcValueEvent::Type::kMemUsed, 20 /* value */},
      {102 /* timestamp */, ArcValueEvent::Type::kSwapRead, 30 /* value */},
      {103 /* timestamp */, ArcValueEvent::Type::kSwapWrite, 40 /* value */},
      {104 /* timestamp */, ArcValueEvent::Type::kSwapWait, 50 /* value */}};

  const base::ListValue value = SerializeValueEvents(events);

  ValueEvents loaded_events;
  EXPECT_TRUE(LoadValueEvents(&value, &loaded_events));
  EXPECT_EQ(events, loaded_events);
}

}  // namespace arc
