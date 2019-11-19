// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/throttle_observer.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/template_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ThrottleObserverTest
    : public testing::Test,
      public base::SupportsWeakPtr<ThrottleObserverTest> {
 public:
  ThrottleObserverTest() {
    observer_.StartObserving(
        nullptr /* content::BrowserContext* */,
        base::BindRepeating(&ThrottleObserverTest::OnObserverStateChanged,
                            AsWeakPtr()));
  }

  void OnObserverStateChanged() { notify_count_++; }

 protected:
  ThrottleObserver* observer() { return &observer_; }
  size_t notify_count() const { return notify_count_; }

 private:
  ThrottleObserver observer_{ThrottleObserver::PriorityLevel::LOW,
                             "TestObserver"};
  size_t notify_count_{0};

  DISALLOW_COPY_AND_ASSIGN(ThrottleObserverTest);
};

// Tests that ThrottleObserver can be constructed and destructed.
TEST_F(ThrottleObserverTest, TestConstructDestruct) {}

// Tests that ThrottleObserver notifies observers only when its 'active'
// state changes
TEST_F(ThrottleObserverTest, TestSetActive) {
  EXPECT_EQ(0U, notify_count());
  EXPECT_FALSE(observer()->active());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(1U, notify_count());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(1U, notify_count());

  observer()->SetActive(false);
  EXPECT_FALSE(observer()->active());
  EXPECT_EQ(2U, notify_count());
}

}  // namespace chromeos
