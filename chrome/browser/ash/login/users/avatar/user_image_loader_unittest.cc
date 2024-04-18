// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "components/user_manager/user_image/user_image.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Points to a webp file with 3 frames of red, green, blue solid colors,
// respectively.
constexpr std::string_view kUserAvatarWebpRelativePath =
    "chromeos/avatars/avatar.webp";

}  // namespace

class UserImageLoaderTest : public testing::Test {
 public:
  UserImageLoaderTest() = default;
  UserImageLoaderTest(const UserImageLoaderTest&) = delete;
  UserImageLoaderTest& operator=(const UserImageLoaderTest&) = delete;
  ~UserImageLoaderTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(UserImageLoaderTest, StartWithFilePathAnimated) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));

  const base::FilePath image_path =
      test_dir.Append(kUserAvatarWebpRelativePath);

  std::string original_contents;
  base::ReadFileToString(image_path, &original_contents);

  base::RunLoop run_loop;
  user_image_loader::StartWithFilePathAnimated(
      task_environment_.GetMainThreadTaskRunner(), image_path,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<user_manager::UserImage> user_image) {
            EXPECT_EQ(
                original_contents,
                std::string(base::as_string_view(*user_image->image_bytes())));
            EXPECT_EQ(user_manager::UserImage::FORMAT_WEBP,
                      user_image->image_format());
            EXPECT_EQ(16, user_image->image().width());
            EXPECT_EQ(16, user_image->image().height());
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace ash
