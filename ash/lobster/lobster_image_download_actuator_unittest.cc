// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_download_actuator.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "lobster_image_download_actuator.h"
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

class LobsterImageDownloadActuatorTest : public AshTestBase {
 public:
  LobsterImageDownloadActuatorTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath Path(const std::string& filename) {
    return scoped_temp_dir_.GetPath().AppendASCII(filename);
  }

  LobsterImageDownloadActuator& download_actuator() {
    return download_actuator_;
  }

 private:
  LobsterImageDownloadActuator download_actuator_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(LobsterImageDownloadActuatorTest, WritingImageToPathCreatesNewFile) {
  std::string data;
  base::test::TestFuture<const LobsterImageDownloadResponse&> future;

  download_actuator().WriteImageToPath(Path("./"), "dummy_image", 0, "a1b2c3",
                                       future.GetCallback());

  EXPECT_TRUE(future.Get().success);
  EXPECT_TRUE(base::PathExists(Path("./dummy_image.jpeg")));
  ASSERT_TRUE(base::ReadFileToString(Path("./dummy_image.jpeg"), &data));
  EXPECT_EQ(data, "a1b2c3");
}

TEST_F(LobsterImageDownloadActuatorTest,
       WriteMultipleImagesWithSameNameCreatesNewFilesWithSuffix) {
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      first_download_future;
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      second_download_future;
  base::test::TestFuture<const LobsterImageDownloadResponse&>
      third_download_future;
  std::string data_file_1, data_file_2, data_file_3;

  ASSERT_FALSE(base::PathExists(Path("./dummy_image.jpeg")));
  ASSERT_FALSE(base::PathExists(Path("./dummy_image-1.jpeg")));
  ASSERT_FALSE(base::PathExists(Path("./dummy_image-2.jpeg")));

  // Write the images to disk.
  download_actuator().WriteImageToPath(Path("./"), "dummy_image", 0, "a1b2c3",
                                       first_download_future.GetCallback());
  download_actuator().WriteImageToPath(Path("./"), "dummy_image", 0, "d4e5f6",
                                       second_download_future.GetCallback());
  download_actuator().WriteImageToPath(Path("./"), "dummy_image", 0, "g7h8i9",
                                       third_download_future.GetCallback());

  EXPECT_TRUE(first_download_future.Get().success);
  EXPECT_TRUE(second_download_future.Get().success);
  EXPECT_TRUE(third_download_future.Get().success);

  EXPECT_TRUE(base::PathExists(Path("./dummy_image.jpeg")));
  EXPECT_TRUE(base::PathExists(Path("./dummy_image-1.jpeg")));
  EXPECT_TRUE(base::PathExists(Path("./dummy_image-2.jpeg")));

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
