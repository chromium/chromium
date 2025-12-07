// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/pasteboard_changed_observation.h"

#import <AppKit/AppKit.h>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "testing/platform_test.h"

namespace base {
namespace {

using PasteboardChangedObservationTest = PlatformTest;

TEST_F(PasteboardChangedObservationTest, TestTriggers) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(2)};
  RunLoop run_loop;

  bool callback_called = false;
  CallbackListSubscription subscription = RegisterPasteboardChangedCallback(
      BindLambdaForTesting([&callback_called, &run_loop] {
        callback_called = true;
        run_loop.Quit();
      }));

  [NSPasteboard.generalPasteboard clearContents];
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(PasteboardChangedObservationTest, TestDoesntTrigger) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(5)};
  RunLoop run_loop;

  int callbacks = 0;
  CallbackListSubscription subscription = RegisterPasteboardChangedCallback(
      BindLambdaForTesting([&callbacks] { ++callbacks; }));

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&run_loop] { run_loop.Quit(); }),
      base::Seconds(3));

  run_loop.Run();
  // Three is arbitrary, but expecting zero is asking for flakiness.
  EXPECT_LE(callbacks, 3);
}

}  // namespace
}  // namespace base
