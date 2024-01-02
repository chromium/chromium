// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media_request.h"

#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

// Any arbitrary insertion timeout.
constexpr base::TimeDelta kInsertionTimeout = base::Seconds(1);

class PickerInsertMediaRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerInsertMediaRequestTest, DoesNotInsertTextWhenBlurred) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_EQ(client.text(), u"");
}

TEST_F(PickerInsertMediaRequestTest, InsertsTextOnNextFocusWhileBlurred) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello", kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest,
       InsertsTextOnNextFocusBeforeTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest,
       DoesNotInsertTextAfterTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_F(PickerInsertMediaRequestTest, InsertsTextOnNextFocusWhileFocused) {
  ui::FakeTextInputClient prev_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::FakeTextInputClient next_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, u"hello", kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest,
       InsertsTextOnNextFocusBeforeTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::FakeTextInputClient next_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest,
       DoesNotInsertTextOnNextFocusAfterTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::FakeTextInputClient next_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), u"");
}

TEST_F(PickerInsertMediaRequestTest, InsertIsCancelledUponDestruction) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  {
    PickerInsertMediaRequest request(&input_method, u"hello",
                                     kInsertionTimeout);
  }
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_F(PickerInsertMediaRequestTest, DoesNotInsertInInputTypeNone) {
  ui::FakeTextInputClient client_none(ui::TEXT_INPUT_TYPE_NONE);
  ui::FakeTextInputClient client_text(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello", kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client_none);
  input_method.SetFocusedTextInputClient(&client_text);

  EXPECT_EQ(client_none.text(), u"");
  EXPECT_EQ(client_text.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest, InsertsOnlyOnceWithMultipleFocus) {
  ui::FakeTextInputClient client1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::FakeTextInputClient client2(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello", kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client1);
  input_method.SetFocusedTextInputClient(&client2);

  EXPECT_EQ(client1.text(), u"hello");
  EXPECT_EQ(client2.text(), u"");
}

TEST_F(PickerInsertMediaRequestTest, InsertsOnlyOnceWithTimeout) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, u"hello",
                                   /*insert_timeout=*/base::Seconds(1));
  input_method.SetFocusedTextInputClient(&client);
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_EQ(client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest, InsertsOnlyOnceWithDestruction) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh input_method(nullptr);

  {
    PickerInsertMediaRequest request(&input_method, u"hello",
                                     kInsertionTimeout);
    input_method.SetFocusedTextInputClient(&client);
  }

  EXPECT_EQ(client.text(), u"hello");
}

TEST_F(PickerInsertMediaRequestTest, DoesNotInsertWhenInputMethodIsDestroyed) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  auto old_input_method = std::make_unique<InputMethodAsh>(nullptr);

  PickerInsertMediaRequest request(old_input_method.get(), u"hello",
                                   kInsertionTimeout);
  old_input_method.reset();
  InputMethodAsh new_input_method(nullptr);
  new_input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

}  // namespace
}  // namespace ash
