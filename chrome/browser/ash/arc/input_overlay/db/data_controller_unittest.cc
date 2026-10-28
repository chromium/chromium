// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/db/data_controller.h"

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc::input_overlay {

class DataControllerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DataControllerTest, TestGetFilePathFromPackageName) {
  TestingProfile profile;
  DataController data_controller(profile,
                                 task_environment_.GetMainThreadTaskRunner());
  const base::FilePath storage_dir =
      profile.GetProfileKey()->GetPath().Append("google_gio");

  // A package name maps to a file directly inside of the storage directory.
  EXPECT_EQ(storage_dir.Append("com.package.name"),
            data_controller.GetFilePathFromPackageName("com.package.name"));

  // Package names which are not a single path component inside of the storage
  // directory are rejected.
  EXPECT_TRUE(data_controller.GetFilePathFromPackageName("").empty());
  EXPECT_TRUE(data_controller.GetFilePathFromPackageName(".").empty());
  EXPECT_TRUE(data_controller.GetFilePathFromPackageName("..").empty());
  EXPECT_TRUE(
      data_controller.GetFilePathFromPackageName("../../MyFiles/name").empty());
  EXPECT_TRUE(
      data_controller.GetFilePathFromPackageName("name/../../name").empty());
  EXPECT_TRUE(data_controller.GetFilePathFromPackageName("dir/name").empty());
  EXPECT_TRUE(
      data_controller.GetFilePathFromPackageName("/absolute/name").empty());
}

TEST_F(DataControllerTest, TestEmptyFilePath) {
  EXPECT_FALSE(DataController::ReadProtoFromFile(base::FilePath()));
  EXPECT_FALSE(DataController::WriteProtoToFile(
      std::make_unique<AppDataProto>(), base::FilePath()));
}

}  // namespace arc::input_overlay
