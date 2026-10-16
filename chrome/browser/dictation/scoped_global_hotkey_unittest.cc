// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/scoped_global_hotkey.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/dictation/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace dictation {
namespace {

ui::Accelerator TestAccelerator() {
  return ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
}

class DictationScopedGlobalHotkeyTest : public testing::Test {
 protected:
  // Presses are handled asynchronously, so run tasks posted so far.
  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  FakeGlobalAcceleratorListener listener_;
  base::MockRepeatingClosure callback_;
};

TEST_F(DictationScopedGlobalHotkeyTest, RegistersAndUnregisters) {
  {
    ScopedGlobalHotkey hotkey(&listener_, TestAccelerator(), callback_.Get());
    EXPECT_TRUE(hotkey.is_registered());
    EXPECT_TRUE(listener_.IsListeningFor(TestAccelerator()));
  }
  EXPECT_FALSE(listener_.IsListeningFor(TestAccelerator()));
}

TEST_F(DictationScopedGlobalHotkeyTest, PressRunsCallback) {
  ScopedGlobalHotkey hotkey(&listener_, TestAccelerator(), callback_.Get());
  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  listener_.Press(TestAccelerator());
  run_loop.Run();
}

TEST_F(DictationScopedGlobalHotkeyTest, CallbackMayDestroyHotkey) {
  std::unique_ptr<ScopedGlobalHotkey> hotkey;
  base::RunLoop run_loop;
  hotkey = std::make_unique<ScopedGlobalHotkey>(&listener_, TestAccelerator(),
                                                base::BindLambdaForTesting([&] {
                                                  hotkey.reset();
                                                  run_loop.Quit();
                                                }));
  listener_.Press(TestAccelerator());
  run_loop.Run();
  EXPECT_FALSE(hotkey);
  EXPECT_FALSE(listener_.IsListeningFor(TestAccelerator()));
}

TEST_F(DictationScopedGlobalHotkeyTest, DestroyedBeforePressIsHandled) {
  auto hotkey = std::make_unique<ScopedGlobalHotkey>(
      &listener_, TestAccelerator(), callback_.Get());
  EXPECT_CALL(callback_, Run()).Times(0);
  listener_.Press(TestAccelerator());
  hotkey.reset();
  RunPostedTasks();
}

TEST_F(DictationScopedGlobalHotkeyTest, NullListenerIsNoOp) {
  ScopedGlobalHotkey hotkey(nullptr, TestAccelerator(), callback_.Get());
  EXPECT_FALSE(hotkey.is_registered());
}

TEST_F(DictationScopedGlobalHotkeyTest, TakenByOtherAppIsNotRegistered) {
  listener_.set_fail_registration(true);
  ScopedGlobalHotkey hotkey(&listener_, TestAccelerator(), callback_.Get());
  EXPECT_FALSE(hotkey.is_registered());
}

TEST_F(DictationScopedGlobalHotkeyTest, TakenByOtherObserverIsNotRemoved) {
  ScopedGlobalHotkey first(&listener_, TestAccelerator(), callback_.Get());
  base::MockRepeatingClosure second_callback;
  {
    ScopedGlobalHotkey second(&listener_, TestAccelerator(),
                              second_callback.Get());
    EXPECT_FALSE(second.is_registered());
  }
  // Destroying `second` must not remove `first`'s registration.
  EXPECT_TRUE(listener_.IsListeningFor(TestAccelerator()));
  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(second_callback, Run()).Times(0);
  listener_.Press(TestAccelerator());
  run_loop.Run();
}

TEST_F(DictationScopedGlobalHotkeyTest, HandledExternallyIsNotRegistered) {
  listener_.set_handled_externally(true);
  ScopedGlobalHotkey hotkey(&listener_, TestAccelerator(), callback_.Get());
  EXPECT_FALSE(hotkey.is_registered());
}

}  // namespace
}  // namespace dictation
