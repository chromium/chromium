// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class PluginVmFilesTest : public testing::Test {
 public:
  void Callback(bool expected, const base::FilePath& dir, bool result) {
    EXPECT_EQ(dir, my_files_.Append("PvmDefault"));
    EXPECT_EQ(result, expected);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    fake_release_ =
        std::make_unique<chromeos::ScopedSetRunningOnChromeOSForTesting>(
            kLsbRelease, base::Time());
    my_files_ = file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  }

  void TearDown() override {
    fake_release_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<chromeos::ScopedSetRunningOnChromeOSForTesting> fake_release_;
  base::FilePath my_files_;
};

TEST_F(PluginVmFilesTest, DirNotExists) {
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, DirAlreadyExists) {
  base::CreateDirectory(my_files_.Append("PvmDefault"));
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, FileAlreadyExists) {
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  base::FilePath path = my_files.Append("PvmDefault");
  EXPECT_TRUE(base::CreateDirectory(my_files));
  EXPECT_EQ(base::WriteFile(path, "", 0), 0);
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), false));
  task_environment_.RunUntilIdle();
}

}  // namespace plugin_vm
