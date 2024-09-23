// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/feed_service_factory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace internal {

TEST(FeedServiceFactoryTest, GetFollowingFeedFollowCountGroupName) {
  EXPECT_EQ("None", GetFollowingFeedFollowCountGroupName(0));
  EXPECT_EQ("1-4", GetFollowingFeedFollowCountGroupName(1));
  EXPECT_EQ("1-4", GetFollowingFeedFollowCountGroupName(4));
  EXPECT_EQ("5-8", GetFollowingFeedFollowCountGroupName(5));
  EXPECT_EQ("5-8", GetFollowingFeedFollowCountGroupName(8));
  EXPECT_EQ("9-12", GetFollowingFeedFollowCountGroupName(9));
  EXPECT_EQ("9-12", GetFollowingFeedFollowCountGroupName(12));
  EXPECT_EQ("13-20", GetFollowingFeedFollowCountGroupName(13));
  EXPECT_EQ("13-20", GetFollowingFeedFollowCountGroupName(20));
  EXPECT_EQ("21+", GetFollowingFeedFollowCountGroupName(21));
  EXPECT_EQ("21+", GetFollowingFeedFollowCountGroupName(9000));
}

}  // namespace internal
}  // namespace feed
