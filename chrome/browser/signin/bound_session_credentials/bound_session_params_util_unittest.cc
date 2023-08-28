// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bound_session_credentials {

TEST(BoundSessionParamsUtilTest, Timestamp) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(987984);  // arbitrary
  EXPECT_EQ(TimestampToTime(TimeToTimestamp(time)), time);
}

}  // namespace bound_session_credentials
