// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include "chrome/browser/browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ChromeOsFeedbackDelegateTest : public testing::Test {
 public:
  ChromeOsFeedbackDelegateTest() {}
  ~ChromeOsFeedbackDelegateTest() override = default;

 protected:
  ChromeOsFeedbackDelegate feedback_delegate_;
};

// Test GetApplicationLocale returns a valid locale.
TEST_F(ChromeOsFeedbackDelegateTest, GetApplicationLocale) {
  EXPECT_EQ(feedback_delegate_.GetApplicationLocale(), "en");
}

}  // namespace ash
