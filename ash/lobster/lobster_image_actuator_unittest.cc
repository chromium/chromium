// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_actuator.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "url/gurl.h"

namespace ash {
namespace {

std::u16string ReadHTMLFromClipboard(ui::Clipboard* clipboard) {
  std::u16string markup;
  std::string url;
  uint32_t start, end;

  clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
                      &markup, &url, &start, &end);
  return markup;
}

class LobsterImageActuatorTest : public AshTestBase {
 public:
  LobsterImageActuatorTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  ui::InputMethod& ime() { return ime_; }

  base::FilePath Path(const std::string& filename) {
    return scoped_temp_dir_.GetPath().AppendASCII(filename);
  }

 private:
  InputMethodAsh ime_{nullptr};
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(LobsterImageActuatorTest, CanInsertImageIntoEligibleTextInputField) {
  ui::FakeTextInputClient text_input_client(&ime(), {.can_insert_image = true});
  base::test::TestFuture<bool> future;

  EXPECT_TRUE(InsertImageOrCopyToClipboard(&text_input_client,
                                           /*image_bytes=*/"a1b2c3"));

  EXPECT_EQ(text_input_client.last_inserted_image_url(),
            GURL(base::StrCat(
                {"data:image/jpeg;base64,", base::Base64Encode("a1b2c3")})));
  EXPECT_FALSE(ash::ToastManager::Get()->IsToastShown("lobster_toast"));
}

TEST_F(LobsterImageActuatorTest,
       FallbackToCopyToClipboardInIneligibleTextInputField) {
  ui::FakeTextInputClient text_input_client(&ime(),
                                            {.can_insert_image = false});

  EXPECT_FALSE(InsertImageOrCopyToClipboard(&text_input_client,
                                            /*image_bytes=*/"a1b2c3"));

  EXPECT_EQ(text_input_client.last_inserted_image_url(), std::nullopt);
  EXPECT_EQ(
      ReadHTMLFromClipboard(ui::Clipboard::GetForCurrentThread()),
      base::StrCat({u"<img src=\"data:image/jpeg;base64,",
                    base::UTF8ToUTF16(base::Base64Encode("a1b2c3")), u"\">"}));
  EXPECT_TRUE(ash::ToastManager::Get()->IsToastShown("lobster_toast"));
}

TEST_F(LobsterImageActuatorTest, WritingImageToPathCreatesNewFile) {
  std::string data;
  base::test::TestFuture<const LobsterImageDownloadResponse&> future;

  WriteImageToPath(Path("./"), "dummy_image", 0, "a1b2c3",
                   future.GetCallback());

  EXPECT_TRUE(future.Get().success);
  EXPECT_TRUE(base::PathExists(Path("./dummy_image.jpeg")));
  ASSERT_TRUE(base::ReadFileToString(Path("./dummy_image.jpeg"), &data));
  EXPECT_EQ(data, "a1b2c3");
}

TEST_F(LobsterImageActuatorTest,
       WriteImageWithExistingPathCreatesNewFileWithSuffix) {
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      first_download_future;
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      second_download_future;
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      third_download_future;

  // Write the first image to disk.
  WriteImageToPath(Path("./"), "dummy_image", 0, "a1b2c3",
                   first_download_future.GetCallback());
  EXPECT_TRUE(first_download_future.Get().success);
  EXPECT_TRUE(base::PathExists(Path("./dummy_image.jpeg")));

  // Write the second image to disk with the same query.
  ASSERT_FALSE(base::PathExists(Path("./dummy_image-1.jpeg")));

  WriteImageToPath(Path("./"), "dummy_image", 0, "d4e5f6",
                   second_download_future.GetCallback());
  EXPECT_TRUE(second_download_future.Get().success);
  EXPECT_TRUE(base::PathExists(Path("./dummy_image-1.jpeg")));

  // Write the third image to disk with the same query.
  ASSERT_FALSE(base::PathExists(Path("./dummy_image-2.jpeg")));

  WriteImageToPath(Path("./"), "dummy_image", 0, "g7h8i9",
                   third_download_future.GetCallback());
  EXPECT_TRUE(third_download_future.Get().success);
  EXPECT_TRUE(base::PathExists(Path("./dummy_image-2.jpeg")));

  // Checks if the file contents are correct and not overridden.
  std::string data_file_1, data_file_2, data_file_3;

  ASSERT_TRUE(base::ReadFileToString(Path("./dummy_image.jpeg"), &data_file_1));
  ASSERT_TRUE(
      base::ReadFileToString(Path("./dummy_image-1.jpeg"), &data_file_2));
  ASSERT_TRUE(
      base::ReadFileToString(Path("./dummy_image-2.jpeg"), &data_file_3));

  EXPECT_EQ(data_file_1, "a1b2c3");
  EXPECT_EQ(data_file_2, "d4e5f6");
  EXPECT_EQ(data_file_3, "g7h8i9");
}

}  // namespace

}  // namespace ash
