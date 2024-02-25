// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media_request.h"

#include <optional>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

// Any arbitrary insertion timeout.
constexpr base::TimeDelta kInsertionTimeout = base::Seconds(1);

struct TestCase {
  // The media data to insert.
  PickerInsertMediaRequest::MediaData data_to_insert;

  // The expected text in the input field if the insertion was successful.
  std::u16string expected_text;

  // The expected image in the input field if the insertion was successful.
  std::optional<GURL> expected_image_url;
};

class PickerInsertMediaRequestTest : public testing::TestWithParam<TestCase> {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerInsertMediaRequestTest,
    testing::Values(
        TestCase{
            .data_to_insert =
                PickerInsertMediaRequest::MediaData::Text(u"hello"),
            .expected_text = u"hello",
        },
        TestCase{
            .data_to_insert = PickerInsertMediaRequest::MediaData::Image(
                GURL("http://foo.com/fake.jpg")),
            .expected_image_url = GURL("http://foo.com/fake.jpg"),
        },
        TestCase{
            .data_to_insert = PickerInsertMediaRequest::MediaData::Link(
                GURL("http://foo.com")),
            .expected_text = u"http://foo.com/",
        }));

TEST_P(PickerInsertMediaRequestTest, DoesNotInsertWhenBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_EQ(client.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, InsertsWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest,
       InsertsOnNextFocusBeforeTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest, DoesNotInsertAfterTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, InsertsOnNextFocusWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), GetParam().expected_text);
  EXPECT_EQ(next_client.last_inserted_image_url(),
            GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest,
       InsertsOnNextFocusBeforeTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), GetParam().expected_text);
  EXPECT_EQ(next_client.last_inserted_image_url(),
            GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest,
       DoesNotInsertOnNextFocusAfterTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, InsertIsCancelledUponDestruction) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  {
    PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                     kInsertionTimeout);
  }
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, DoesNotInsertInInputTypeNone) {
  ui::FakeTextInputClient client_none(ui::TEXT_INPUT_TYPE_NONE);
  ui::FakeTextInputClient client_text(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client_none);
  input_method.SetFocusedTextInputClient(&client_text);

  EXPECT_EQ(client_none.text(), u"");
  EXPECT_EQ(client_text.text(), GetParam().expected_text);
}

TEST_P(PickerInsertMediaRequestTest, InsertsOnlyOnceWithMultipleFocus) {
  ui::FakeTextInputClient client1(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient client2(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   kInsertionTimeout);
  input_method.SetFocusedTextInputClient(&client1);
  input_method.SetFocusedTextInputClient(&client2);

  EXPECT_EQ(client1.text(), GetParam().expected_text);
  EXPECT_EQ(client2.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, InsertsOnlyOnceWithTimeout) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1));
  input_method.SetFocusedTextInputClient(&client);
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest, InsertsOnlyOnceWithDestruction) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  {
    PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                     kInsertionTimeout);
    input_method.SetFocusedTextInputClient(&client);
  }

  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

TEST_P(PickerInsertMediaRequestTest, DoesNotInsertWhenInputMethodIsDestroyed) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  auto old_input_method = std::make_unique<InputMethodAsh>(nullptr);

  PickerInsertMediaRequest request(
      old_input_method.get(), GetParam().data_to_insert, kInsertionTimeout);
  old_input_method.reset();
  InputMethodAsh new_input_method(nullptr);
  new_input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_P(PickerInsertMediaRequestTest, CallsFailureCallbackOnTimeout) {
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<void> failure_future;
  PickerInsertMediaRequest request(&input_method, GetParam().data_to_insert,
                                   /*insert_timeout=*/base::Seconds(1),
                                   failure_future.GetCallback());
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(failure_future.Wait());
}

TEST(PickerInsertMediaRequestUnsupportedTest,
     InsertingUnsupportedImageCallsFailureCallback) {
  base::test::SingleThreadTaskEnvironment task_environment;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<void> failure_future;
  PickerInsertMediaRequest request(
      &input_method,
      PickerInsertMediaRequest::MediaData::Image(GURL("http://foo.com")),
      /*insert_timeout=*/base::Seconds(1), failure_future.GetCallback());
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_TRUE(failure_future.Wait());
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

}  // namespace
}  // namespace ash
