// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)

#include "base/message_loop/message_pump_apple.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/platform_test.h"

namespace base {

namespace {

class MessagePumpAppleInitialNestingTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // Ensure a clean state for each test.
    MessagePumpUIApplication::ResetNextInitialNestingLevelForTesting();
  }

  void TearDown() override {
    // Clean up after each test to avoid state leakage.
    MessagePumpUIApplication::ResetNextInitialNestingLevelForTesting();
    PlatformTest::TearDown();
  }
};

class TestMessagePump : public MessagePumpUIApplication {
 public:
  using MessagePumpCFRunLoopBase::nesting_level;
  using MessagePumpCFRunLoopBase::run_nesting_level;
  void SimulateExit() {
    EnterExitObserver(/*observer=*/nullptr, kCFRunLoopExit, this);
  }
};

class PlaceholderDelegate : public MessagePump::Delegate {
 public:
  void BeforeWait() override {}
  void BeginNativeWorkBeforeDoWork() override {}
  int RunDepth() override { return 1; }
  void OnBeginWorkItem() override {}
  void OnEndWorkItem(int) override {}
  NextWorkInfo DoWork() override { return NextWorkInfo{TimeTicks::Max()}; }
  void DoIdleWork() override {}
};

}  // namespace

TEST_F(MessagePumpAppleInitialNestingTest, DefaultNestingLevel) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Default behavior (no call to SetInitialNestingLevel).
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(1, pump.nesting_level());
  EXPECT_EQ(0, pump.run_nesting_level());
  pump.Detach();
}

TEST_F(MessagePumpAppleInitialNestingTest, SetInitialNestingLevelOne) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Test SetNextInitialNestingLevelForCurrentThread(1), which should behave like
  // the default.
  MessagePumpUIApplication::SetNextInitialNestingLevelForCurrentThread(1);
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(1, pump.nesting_level());
  EXPECT_EQ(0, pump.run_nesting_level());
  pump.Detach();
}

TEST_F(MessagePumpAppleInitialNestingTest, SetInitialNestingLevelTwo) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Test SetNextInitialNestingLevelForCurrentThread with a value greater than 1.
  MessagePumpUIApplication::SetNextInitialNestingLevelForCurrentThread(2);
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(2, pump.nesting_level());
  EXPECT_EQ(0, pump.run_nesting_level());
  pump.Detach();
}

TEST_F(MessagePumpAppleInitialNestingTest, InitialNestingLevelResetsAfterUse) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  MessagePumpUIApplication::SetNextInitialNestingLevelForCurrentThread(2);
  {
    TestMessagePump pump;
    PlaceholderDelegate delegate;
    pump.Attach(&delegate);
    pump.Detach();
  }

  // Verify that the level is reset to default (1) after being used.
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(1, pump.nesting_level());
  EXPECT_EQ(0, pump.run_nesting_level());
  pump.Detach();
}

TEST_F(MessagePumpAppleInitialNestingTest, Unwinding) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Test SetNextInitialNestingLevelForCurrentThread with a value of 4 and check
  // unwinding.
  MessagePumpUIApplication::SetNextInitialNestingLevelForCurrentThread(4);
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(4, pump.nesting_level());
  EXPECT_EQ(0, pump.run_nesting_level());

  // Simulate a pre-existing nested loop exiting.
  pump.SimulateExit();

  // nesting_level should have decremented from 4 to 3.
  EXPECT_EQ(3, pump.nesting_level());
  // run_nesting_level remains 0.
  EXPECT_EQ(0, pump.run_nesting_level());

  pump.Detach();
}

}  // namespace base

#endif  // BUILDFLAG(IS_IOS)
