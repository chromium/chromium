// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests the environment for packing extensions from the command line
// when using the --pack-extension switch.
class PackExtensionTest : public testing::Test {
 public:
  PackExtensionTest() {
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  }

 protected:
  bool TestPackExtension(const base::FilePath& path) {
    base::ScopedTempDir temp_dir;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
    EXPECT_TRUE(base::CopyDirectory(path, temp_dir.GetPath(), true));
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchPath(switches::kPackExtension,
                                  temp_dir.GetPath().Append(path.BaseName()));
    std::string error_message;
    bool result = startup_helper_.PackExtension(command_line, &error_message);
    EXPECT_EQ(result, error_message.empty()) << error_message;
    return result;
  }

  content::BrowserTaskEnvironment task_environment_;

  base::FilePath test_data_dir_;
  StartupHelper startup_helper_;
};

TEST_F(PackExtensionTest, Extension) {
  ASSERT_TRUE(TestPackExtension(test_data_dir_.AppendASCII("api_test")
                                              .AppendASCII("tabs")
                                              .AppendASCII("basics")));
}

TEST_F(PackExtensionTest, ExtensionWithManagedStorage) {
  ASSERT_TRUE(TestPackExtension(test_data_dir_.AppendASCII("api_test")
                                    .AppendASCII("settings")
                                    .AppendASCII("managed_storage")));
}

TEST_F(PackExtensionTest, PackagedApp) {
  ASSERT_TRUE(TestPackExtension(test_data_dir_.AppendASCII("packaged_app")));
}

TEST_F(PackExtensionTest, PlatformApp) {
  ASSERT_TRUE(TestPackExtension(test_data_dir_.AppendASCII("platform_apps")
                                              .AppendASCII("minimal")));
}

}  // namespace extensions
