// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool_request.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

std::optional<int64_t> ParseMs(std::string_view timecode) {
  std::optional<base::TimeDelta> seek_time =
      SeekMediaToolRequest::FromTimecode(timecode);
  if (!seek_time) {
    return std::nullopt;
  }
  return seek_time->InMilliseconds();
}

TEST(SeekMediaToolRequestTest, FromTimecodeValid) {
  EXPECT_EQ(ParseMs("0"), 0);
  EXPECT_EQ(ParseMs("30"), 30'000);
  EXPECT_EQ(ParseMs("90"), 90'000);
  EXPECT_EQ(ParseMs("1:45"), 105'000);
  EXPECT_EQ(ParseMs("90:00"), 5'400'000);
  EXPECT_EQ(ParseMs("1:02:15"), 3'735'000);
  EXPECT_EQ(ParseMs("0:00:59"), 59'000);
  EXPECT_EQ(ParseMs(" 1 : 02 "), 62'000);
}

TEST(SeekMediaToolRequestTest, FromTimecodeInvalid) {
  // Malformed.
  EXPECT_EQ(ParseMs(""), std::nullopt);
  EXPECT_EQ(ParseMs("abc"), std::nullopt);
  EXPECT_EQ(ParseMs("1.5"), std::nullopt);
  EXPECT_EQ(ParseMs("1:2:3:4"), std::nullopt);
  // Empty fields.
  EXPECT_EQ(ParseMs(":30"), std::nullopt);
  EXPECT_EQ(ParseMs("1:"), std::nullopt);
  EXPECT_EQ(ParseMs("1::30"), std::nullopt);
  // Out-of-range minutes/seconds.
  EXPECT_EQ(ParseMs("1:60"), std::nullopt);
  EXPECT_EQ(ParseMs("1:60:00"), std::nullopt);
  EXPECT_EQ(ParseMs("1:00:60"), std::nullopt);
  // Negative values.
  EXPECT_EQ(ParseMs("-1"), std::nullopt);
  EXPECT_EQ(ParseMs("-1:30"), std::nullopt);
  EXPECT_EQ(ParseMs("1:-1"), std::nullopt);
  EXPECT_EQ(ParseMs("-1:00:00"), std::nullopt);
}

}  // namespace
}  // namespace actor
