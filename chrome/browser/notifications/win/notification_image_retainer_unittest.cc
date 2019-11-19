// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_image_retainer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

// This value has to stay in sync with that in notification_image_retainer.cc.
constexpr base::TimeDelta kDeletionDelay = base::TimeDelta::FromSeconds(12);

}  // namespace

class NotificationImageRetainerTest : public ::testing::Test {
 public:
  NotificationImageRetainerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  ~NotificationImageRetainerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  base::ScopedPathOverride user_data_dir_override_;

  DISALLOW_COPY_AND_ASSIGN(NotificationImageRetainerTest);
};

TEST_F(NotificationImageRetainerTest, RegisterTemporaryImage) {
  auto image_retainer = std::make_unique<NotificationImageRetainer>(
      task_environment_.GetMainThreadTaskRunner(),
      task_environment_.GetMockTickClock());

  SkBitmap icon;
  icon.allocN32Pixels(64, 64);
  icon.eraseARGB(255, 100, 150, 200);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(icon);

  base::FilePath temp_file = image_retainer->RegisterTemporaryImage(image);
  ASSERT_FALSE(temp_file.empty());
  ASSERT_TRUE(base::PathExists(temp_file));

  // Fast-forward the task runner so that the file deletion task posted in
  // RegisterTemporaryImage() finishes running.
  task_environment_.FastForwardBy(kDeletionDelay);

  // The temp file should be deleted now.
  ASSERT_FALSE(base::PathExists(temp_file));

  // The destruction of the image retainer object won't delete the image
  // directory.
  image_retainer.reset();
  ASSERT_TRUE(base::PathExists(temp_file.DirName()));
}

TEST_F(NotificationImageRetainerTest, DeleteFilesInBatch) {
  auto image_retainer = std::make_unique<NotificationImageRetainer>(
      task_environment_.GetMainThreadTaskRunner(),
      task_environment_.GetMockTickClock());

  SkBitmap icon;
  icon.allocN32Pixels(64, 64);
  icon.eraseARGB(255, 100, 150, 200);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(icon);

  // Create 1st image file on disk.
  base::FilePath temp_file1 = image_retainer->RegisterTemporaryImage(image);
  ASSERT_FALSE(temp_file1.empty());
  ASSERT_TRUE(base::PathExists(temp_file1));

  // Simulate ticking of the clock so that the next image file has a different
  // registration time.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Create 2nd image file on disk.
  base::FilePath temp_file2 = image_retainer->RegisterTemporaryImage(image);
  ASSERT_FALSE(temp_file2.empty());
  ASSERT_TRUE(base::PathExists(temp_file2));

  // Simulate ticking of the clock so that the next image file has a different
  // registration time.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Create 3rd image file on disk.
  base::FilePath temp_file3 = image_retainer->RegisterTemporaryImage(image);
  ASSERT_FALSE(temp_file3.empty());
  ASSERT_TRUE(base::PathExists(temp_file3));

  // Fast-forward the task runner by kDeletionDelay. The first temp file should
  // be deleted now, while the other two should still be around.
  task_environment_.FastForwardBy(kDeletionDelay);
  ASSERT_FALSE(base::PathExists(temp_file1));
  ASSERT_TRUE(base::PathExists(temp_file2));
  ASSERT_TRUE(base::PathExists(temp_file3));

  // Fast-forward the task runner again. The second and the third temp files
  // are deleted simultaneously.
  task_environment_.FastForwardBy(kDeletionDelay);
  ASSERT_FALSE(base::PathExists(temp_file2));
  ASSERT_FALSE(base::PathExists(temp_file3));
}

TEST_F(NotificationImageRetainerTest, CleanupFilesFromPrevSessions) {
  auto image_retainer = std::make_unique<NotificationImageRetainer>(
      task_environment_.GetMainThreadTaskRunner(),
      task_environment_.GetMockTickClock());

  const base::FilePath& image_dir = image_retainer->image_dir();
  ASSERT_TRUE(base::CreateDirectory(image_dir));

  // Create two temp files as if they were created in previous sessions.
  base::FilePath temp_file1;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(image_dir, &temp_file1));

  base::FilePath temp_file2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(image_dir, &temp_file2));

  ASSERT_TRUE(base::PathExists(temp_file1));
  ASSERT_TRUE(base::PathExists(temp_file2));

  // Create a new temp file in the current session. This file will be scheduled
  // to delete after kDeletionDelay seconds.
  SkBitmap icon;
  icon.allocN32Pixels(64, 64);
  icon.eraseARGB(255, 100, 150, 200);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(icon);

  base::FilePath temp_file3 = image_retainer->RegisterTemporaryImage(image);
  ASSERT_FALSE(temp_file3.empty());
  ASSERT_TRUE(base::PathExists(temp_file3));

  // Schedule a file cleanup task.
  image_retainer->CleanupFilesFromPrevSessions();

  // Now the file cleanup task finishes running.
  task_environment_.RunUntilIdle();

  // The two temp files from previous sessions should be deleted now.
  ASSERT_FALSE(base::PathExists(temp_file1));
  ASSERT_FALSE(base::PathExists(temp_file2));

  // The temp file created in this session should still be around.
  ASSERT_TRUE(base::PathExists(temp_file3));

  // Fast-forward the task runner so that the file deletion task posted in
  // RegisterTemporaryImage() finishes running.
  task_environment_.FastForwardBy(kDeletionDelay);

  // The temp file created in this session should be deleted now.
  ASSERT_FALSE(base::PathExists(temp_file3));

  // The image directory should still be around.
  ASSERT_TRUE(base::PathExists(image_dir));
}
