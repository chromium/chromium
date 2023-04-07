// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"

#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcWindowObserverTest : public testing::Test {
 public:
  ArcWindowObserverTest() = default;

  ArcWindowObserverTest(const ArcWindowObserverTest&) = delete;
  ArcWindowObserverTest& operator=(const ArcWindowObserverTest&) = delete;

  ~ArcWindowObserverTest() override = default;

  void SetUp() override {
    arc_window_watcher_ = std::make_unique<ash::ArcWindowWatcher>();
    testing_profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    testing_profile_.reset();
    arc_window_watcher_.reset();
  }

 protected:
  ArcWindowObserver* observer() { return &window_observer_; }
  ash::ArcWindowWatcher* watcher() { return ash::ArcWindowWatcher::instance(); }
  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  ArcWindowObserver window_observer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ash::ArcWindowWatcher> arc_window_watcher_;
};

TEST_F(ArcWindowObserverTest, TestConstructDestruct) {}

TEST_F(ArcWindowObserverTest, TestStatusChanges) {
  unittest::ThrottleTestObserver test_observer;
  EXPECT_EQ(0, test_observer.count());

  EXPECT_FALSE(watcher()->HasCountObserver(observer()));
  // base::Unretained below: safe because all involved objects share scope.
  observer()->StartObserving(
      profile(), base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                                     base::Unretained(&test_observer)));
  EXPECT_TRUE(watcher()->HasCountObserver(observer()));

  EXPECT_EQ(1, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());

  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  observer()->OnArcWindowCountChanged(1);
  EXPECT_TRUE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  observer()->OnArcWindowCountChanged(2);
  EXPECT_TRUE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  observer()->OnArcWindowCountChanged(0);
  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  EXPECT_TRUE(watcher()->HasCountObserver(observer()));
  observer()->StopObserving();
  EXPECT_FALSE(watcher()->HasCountObserver(observer()));
}

}  // namespace arc
