// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_event_matcher.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

ArcTracingEvent MakeEvent(const char* json_str) {
  return ArcTracingEvent(
      std::move(base::JSONReader::Read(json_str)->GetDict()));
}

}  // namespace

using ArcTracingEventMatcherTest = testing::Test;

TEST_F(ArcTracingEventMatcherTest, CategoryNameMatch) {
  ArcTracingEventMatcher matcher("exo:Surface::Attach");

  EXPECT_TRUE(
      matcher.Match(MakeEvent(R"({"cat":"exo","name":"Surface::Attach"})")));
  EXPECT_FALSE(
      matcher.Match(MakeEvent(R"({"cat":"exo","name":"Surface::Attac"})")));
  EXPECT_FALSE(
      matcher.Match(MakeEvent(R"({"cat":"exo","name":"Surface::AttacH"})")));
  EXPECT_FALSE(
      matcher.Match(MakeEvent(R"({"cat":"exo","name":"Surface::Attach2"})")));
  EXPECT_FALSE(matcher.Match(
      MakeEvent(R"({"cat":"android","name":"Surface::Attach"})")));
}

TEST_F(ArcTracingEventMatcherTest, CategoryNamePrefixMatch) {
  ArcTracingEventMatcher matcher("android:ARC_VSYNC|*");

  EXPECT_FALSE(
      matcher.Match(MakeEvent(R"({"cat":"android","name":"ARC_VSYNC"})")));
  EXPECT_TRUE(
      matcher.Match(MakeEvent(R"({"cat":"android","name":"ARC_VSYNC|"})")));
  EXPECT_TRUE(
      matcher.Match(MakeEvent(R"({"cat":"android","name":"ARC_VSYNC|123"})")));
  EXPECT_TRUE(
      matcher.Match(MakeEvent(R"({"cat":"android","name":"ARC_VSYNC|abc"})")));
  EXPECT_FALSE(
      matcher.Match(MakeEvent(R"({"cat":"android","name":"XARC_VSYNC|123"})")));
}

TEST_F(ArcTracingEventMatcherTest, CategoryNamePrefixAndroidInt64) {
  ArcTracingEventMatcher matcher("android:ARC_VSYNC|*");

  EXPECT_EQ(std::nullopt, matcher.ReadAndroidEventInt64(
                              MakeEvent(R"({"name":"ARC_VSYNC"})")));
  EXPECT_EQ(std::nullopt, matcher.ReadAndroidEventInt64(
                              MakeEvent(R"({"name":"ARC_VSYNC|"})")));
  EXPECT_EQ(std::nullopt, matcher.ReadAndroidEventInt64(
                              MakeEvent(R"({"name":"ARC_VSYNC|abc"})")));
  EXPECT_EQ(std::make_optional(0), matcher.ReadAndroidEventInt64(
                                       MakeEvent(R"({"name":"ARC_VSYNC|0"})")));
  EXPECT_EQ(std::make_optional(777777777777LL),
            matcher.ReadAndroidEventInt64(
                MakeEvent(R"({"name":"ARC_VSYNC|777777777777"})")));
  EXPECT_EQ(std::make_optional(-777777777777LL),
            matcher.ReadAndroidEventInt64(
                MakeEvent(R"({"name":"ARC_VSYNC|-777777777777"})")));
}

}  // namespace arc
