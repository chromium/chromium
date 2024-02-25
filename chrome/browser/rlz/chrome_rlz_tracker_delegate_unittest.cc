// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeRLZTrackerDelegateTest : public testing::Test {
 public:
  ChromeRLZTrackerDelegateTest()
      : delegate_(std::make_unique<ChromeRLZTrackerDelegate>()) {}
  ~ChromeRLZTrackerDelegateTest() override {}

  ChromeRLZTrackerDelegate* delegate() { return delegate_.get(); }

  MOCK_METHOD0(TestCallback, void(void));

 private:
  std::unique_ptr<ChromeRLZTrackerDelegate> delegate_;
};

TEST_F(ChromeRLZTrackerDelegateTest, HomepageSearchCallback) {
  // The callback is not run if it is not set.
  EXPECT_CALL(*this, TestCallback()).Times(0);
  delegate()->RunHomepageSearchCallback();

  // Sets the callback and runs it.
  delegate()->SetHomepageSearchCallback(base::BindOnce(
      &ChromeRLZTrackerDelegateTest::TestCallback, base::Unretained(this)));
  EXPECT_CALL(*this, TestCallback());
  delegate()->RunHomepageSearchCallback();

  // The callback should be cleared and will not run again.
  EXPECT_CALL(*this, TestCallback()).Times(0);
  delegate()->RunHomepageSearchCallback();
}
