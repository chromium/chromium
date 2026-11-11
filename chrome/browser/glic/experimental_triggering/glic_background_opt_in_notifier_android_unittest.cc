// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_background_opt_in_notifier_android.h"

#include <vector>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicBackgroundOptInNotifierAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    BackgroundOptInNotifier::Get().SetCallbacksForTesting(
        base::BindLambdaForTesting([this]() {
          if (!can_show_) {
            return false;
          }
          ++show_count_;
          is_showing_ = true;
          return true;
        }),
        base::BindLambdaForTesting([this]() {
          ++hide_count_;
          is_showing_ = false;
        }));
  }

  void TearDown() override {
    BackgroundOptInNotifier::Get().SetCallbacksForTesting({}, {});
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  bool can_show_ = true;
  bool is_showing_ = false;
  int show_count_ = 0;
  int hide_count_ = 0;
};

TEST_F(GlicBackgroundOptInNotifierAndroidTest,
       ShowsOnFirstSubscriberAndHidesAfterLastUnsubscribes) {
  auto sub1 = BackgroundOptInNotifier::Get().Subscribe(base::DoNothing());
  EXPECT_TRUE(sub1);
  EXPECT_TRUE(is_showing_);
  EXPECT_EQ(show_count_, 1);
  EXPECT_EQ(hide_count_, 0);

  auto sub2 = BackgroundOptInNotifier::Get().Subscribe(base::DoNothing());
  EXPECT_TRUE(sub2);
  EXPECT_TRUE(is_showing_);
  EXPECT_EQ(show_count_, 1);

  sub1 = {};
  EXPECT_TRUE(is_showing_);
  EXPECT_EQ(hide_count_, 0);

  sub2 = {};
  EXPECT_FALSE(is_showing_);
  EXPECT_EQ(hide_count_, 1);
}

TEST_F(GlicBackgroundOptInNotifierAndroidTest,
       ReturnsEmptySubscriptionWhenShowFails) {
  can_show_ = false;

  auto sub = BackgroundOptInNotifier::Get().Subscribe(base::DoNothing());
  EXPECT_FALSE(sub);
  EXPECT_FALSE(is_showing_);
  EXPECT_EQ(show_count_, 0);
  EXPECT_EQ(hide_count_, 0);
}

TEST_F(GlicBackgroundOptInNotifierAndroidTest,
       ForwardsCancelledEventToSubscribers) {
  std::vector<BackgroundOptInNotifier::Event> received_events;
  auto sub = BackgroundOptInNotifier::Get().Subscribe(
      base::BindLambdaForTesting([&](BackgroundOptInNotifier::Event event) {
        received_events.push_back(event);
      }));
  ASSERT_TRUE(sub);

  BackgroundOptInNotifier::Get().Notify(
      BackgroundOptInNotifier::Event::kCancelled);
  ASSERT_EQ(received_events.size(), 1u);
  EXPECT_EQ(received_events[0], BackgroundOptInNotifier::Event::kCancelled);
}

TEST_F(GlicBackgroundOptInNotifierAndroidTest,
       CoalescesMultipleForegroundSignalsIntoSingleTask) {
  int foreground_count = 0;
  base::test::TestFuture<void> foreground_future;
  auto sub = BackgroundOptInNotifier::Get().Subscribe(
      base::BindLambdaForTesting([&](BackgroundOptInNotifier::Event event) {
        if (event == BackgroundOptInNotifier::Event::kForegrounded) {
          ++foreground_count;
          foreground_future.SetValue();
        }
      }));
  ASSERT_TRUE(sub);

  BackgroundOptInNotifier::Get().ScheduleForegroundNotification();
  BackgroundOptInNotifier::Get().OnBrowserCreated(/*browser=*/nullptr);
  BackgroundOptInNotifier::Get().OnBrowserActivated(/*browser=*/nullptr);
  BackgroundOptInNotifier::Get().DidSelectTab(/*tab=*/nullptr);
  EXPECT_EQ(foreground_count, 0);

  ASSERT_TRUE(foreground_future.Wait());
  foreground_future.Clear();
  EXPECT_EQ(foreground_count, 1);

  BackgroundOptInNotifier::Get().ScheduleForegroundNotification();
  ASSERT_TRUE(foreground_future.Wait());
  EXPECT_EQ(foreground_count, 2);
}

TEST_F(GlicBackgroundOptInNotifierAndroidTest,
       UnsubscribingBeforeScheduledTaskSuppressesNotification) {
  int foreground_count = 0;
  auto sub = BackgroundOptInNotifier::Get().Subscribe(
      base::BindLambdaForTesting([&](BackgroundOptInNotifier::Event event) {
        if (event == BackgroundOptInNotifier::Event::kForegrounded) {
          ++foreground_count;
        }
      }));
  ASSERT_TRUE(sub);

  BackgroundOptInNotifier::Get().ScheduleForegroundNotification();
  sub = {};
  EXPECT_FALSE(is_showing_);

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(foreground_count, 0);
}

}  // namespace
}  // namespace glic
