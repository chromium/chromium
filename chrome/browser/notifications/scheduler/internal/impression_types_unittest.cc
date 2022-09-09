// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Verifies equal operator of ClientState.
TEST(ImpressionTypesTest, ClientStateEqual) {
  ClientState client_state0, client_state1;
  EXPECT_EQ(client_state0, client_state1);
  client_state0.impressions.emplace_back(Impression());
  EXPECT_FALSE(client_state0 == client_state1);
  client_state1.impressions.emplace_back(Impression());
  EXPECT_EQ(client_state0, client_state1);
  client_state0.impressions.front().guid = "guid";
  EXPECT_FALSE(client_state0 == client_state1);
}

}  // namespace
}  // namespace notifications
