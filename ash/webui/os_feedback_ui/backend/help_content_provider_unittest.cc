// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace feedback {

class HelpContentProviderTest : public testing::Test {
 public:
  HelpContentProviderTest() = default;
  ~HelpContentProviderTest() override = default;
};

TEST_F(HelpContentProviderTest, DummyTest) {
  HelpContentProvider provider;
  EXPECT_TRUE(true);
}

}  // namespace feedback
}  // namespace ash
