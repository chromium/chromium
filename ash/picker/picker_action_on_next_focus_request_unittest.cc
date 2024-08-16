// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_action_on_next_focus_request.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

class PickerActionOnNextFocusRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerActionOnNextFocusRequestTest, PerformsActionOnNextFocus) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<void> action_future;
  PickerActionOnNextFocusRequest request(
      &input_method, /*action_timeout=*/base::Seconds(1),
      action_future.GetCallback(), base::DoNothing());
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_TRUE(action_future.Wait());
}

TEST_F(PickerActionOnNextFocusRequestTest,
       PerformsActionOnlyOnceWithMultipleFocus) {
  ui::FakeTextInputClient client1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::FakeTextInputClient client2(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<void> action_future;
  PickerActionOnNextFocusRequest request(
      &input_method, /*action_timeout=*/base::Seconds(1),
      action_future.GetCallback(), base::DoNothing());
  input_method.SetFocusedTextInputClient(&client1);
  input_method.SetFocusedTextInputClient(&client2);

  EXPECT_TRUE(action_future.Wait());
}

TEST_F(PickerActionOnNextFocusRequestTest,
       DoesNotCallTimeoutCallbackAfterSuccessfulAction) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);
  base::test::TestFuture<void> action_future;
  base::test::TestFuture<void> timeout_future;
  PickerActionOnNextFocusRequest request(
      &input_method, /*action_timeout=*/base::Seconds(1),
      action_future.GetCallback(), timeout_future.GetCallback());

  input_method.SetFocusedTextInputClient(&client);
  task_environment().FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(action_future.Wait());
  EXPECT_FALSE(timeout_future.IsReady());
}

TEST_F(PickerActionOnNextFocusRequestTest, CallsTimeoutCallbackOnTimeout) {
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<void> timeout_future;
  PickerActionOnNextFocusRequest request(
      &input_method, /*action_timeout=*/base::Seconds(1), base::DoNothing(),
      timeout_future.GetCallback());
  task_environment().FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(timeout_future.Wait());
}

TEST_F(PickerActionOnNextFocusRequestTest, DoesNotPerformActionAfterTimeout) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);
  base::test::TestFuture<void> action_future;
  base::test::TestFuture<void> timeout_future;
  PickerActionOnNextFocusRequest request(
      &input_method, /*action_timeout=*/base::Seconds(1),
      action_future.GetCallback(), timeout_future.GetCallback());

  task_environment().FastForwardBy(base::Seconds(2));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_FALSE(action_future.IsReady());
  EXPECT_TRUE(timeout_future.Wait());
}

}  // namespace
}  // namespace ash
