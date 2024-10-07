// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include <optional>
#include <string>

#include "ash/picker/picker_rich_media.h"
#include "ash/picker/picker_web_paste_target.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

class ScopedTestFile {
 public:
  bool Create(std::string_view file_name, std::string_view contents) {
    if (!temp_dir_.CreateUniqueTempDir()) {
      return false;
    }
    path_ = temp_dir_.GetPath().Append(file_name);
    if (!base::WriteFile(path_, contents)) {
      return false;
    }
    return true;
  }

  const base::FilePath& path() const { return path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;
};

struct TestCase {
  // The media to insert.
  PickerRichMedia media_to_insert;

  // The expected text in the input field if the insertion was successful.
  std::u16string expected_text;

  // The expected image in the input field if the insertion was successful.
  std::optional<GURL> expected_image_url;
};

using PickerInsertMediaTest = testing::TestWithParam<TestCase>;

TEST_P(PickerInsertMediaTest, SupportsInsertingMedia) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  EXPECT_TRUE(
      InputFieldSupportsInsertingMedia(GetParam().media_to_insert, client));
}

TEST_P(PickerInsertMediaTest, InsertsMediaWithNoError) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(GetParam().media_to_insert, client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kSuccess);
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
            .media_to_insert = PickerImageMedia(GURL("http://foo.com/fake.jpg"),
                                                gfx::Size(10, 10)),
            .expected_image_url = GURL("http://foo.com/fake.jpg"),
        },
        TestCase{
            .media_to_insert = PickerLinkMedia(GURL("http://foo.com"), "foo"),
            .expected_text = u"http://foo.com/",
        }));

TEST(PickerInsertImageMediaTest, UnsupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  EXPECT_FALSE(InputFieldSupportsInsertingMedia(
      PickerImageMedia(GURL("http://foo.com"), gfx::Size(10, 10)), client));
}

TEST(PickerInsertImageMediaTest,
     InsertingUnsupportedInputFieldFailsAsynchronously) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(
      PickerImageMedia(GURL("http://foo.com"), gfx::Size(10, 10)), client,
      /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(PickerInsertLocalFileMediaTest, SupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  EXPECT_TRUE(InputFieldSupportsInsertingMedia(
      PickerLocalFileMedia(base::FilePath("foo.txt")), client));
}

TEST(PickerInsertLocalFileMediaTest, UnsupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  EXPECT_FALSE(InputFieldSupportsInsertingMedia(
      PickerLocalFileMedia(base::FilePath("foo.txt")), client));
}

TEST(PickerInsertLocalFileMediaTest, InsertsAsynchronously) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.png", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(PickerLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kSuccess);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(),
            GURL("data:image/png;base64,VGVzdCBkYXRh"));
}

TEST(PickerInsertLocalFileMediaTest, InsertingInUnsupportedClientReturnsError) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.png", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(PickerLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(PickerInsertLocalFileMediaTest,
     InsertingUnsupportedMediaTypeReturnsError) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.meow", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(PickerLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(PickerInsertLocalFileMediaTest, InsertingNonExistentFileReturnsError) {
  base::test::TaskEnvironment task_environment;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(PickerLocalFileMedia(base::FilePath("foo.txt")),
                          client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kNotFound);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

}  // namespace
}  // namespace ash
