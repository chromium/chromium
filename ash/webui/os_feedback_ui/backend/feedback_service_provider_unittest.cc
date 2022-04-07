// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace feedback {

class FeedbackServiceProviderTest : public testing::Test {
 public:
  FeedbackServiceProviderTest() = default;
  ~FeedbackServiceProviderTest() override = default;
};

TEST_F(FeedbackServiceProviderTest, DummyTest) {
  FeedbackServiceProvider provider;
  EXPECT_TRUE(true);
}

}  // namespace feedback
}  // namespace ash
