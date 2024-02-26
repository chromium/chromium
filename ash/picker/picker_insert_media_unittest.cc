// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include <optional>
#include <string>

#include "ash/picker/picker_rich_media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

struct TestCase {
  // The media to insert.
  PickerRichMedia media_to_insert;

  // The expected text in the input field if the insertion was successful.
  std::u16string expected_text;

  // The expected image in the input field if the insertion was successful.
  std::optional<GURL> expected_image_url;
};

using PickerInsertMediaTest = testing::TestWithParam<TestCase>;

TEST_P(PickerInsertMediaTest, ReturnsFalseForNullClient) {
  EXPECT_FALSE(InsertMediaToInputField(GetParam().media_to_insert,
                                       /*client=*/nullptr));
}

TEST_P(PickerInsertMediaTest, InsertsOnNextFocusWhileFocused) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  EXPECT_TRUE(InsertMediaToInputField(GetParam().media_to_insert, &client));
  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerInsertMediaTest,
    testing::Values(
        TestCase{
            .media_to_insert = PickerTextMedia(u"hello"),
            .expected_text = u"hello",
        },
        TestCase{
            .media_to_insert =
                PickerImageMedia(GURL("http://foo.com/fake.jpg")),
            .expected_image_url = GURL("http://foo.com/fake.jpg"),
        },
        TestCase{
            .media_to_insert = PickerLinkMedia(GURL("http://foo.com")),
            .expected_text = u"http://foo.com/",
        }));

TEST(PickerInsertMediaUnsupportedTest, InsertingUnsupportedImageReturnsFalse) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  EXPECT_FALSE(InsertMediaToInputField(PickerImageMedia(GURL("http://foo.com")),
                                       &client));
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

}  // namespace
}  // namespace ash
