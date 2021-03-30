// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
static const char kActiveUserHash[] = "01234567890";
} // namespace

// The boolean parameter, retrieved by GetParam(), is true if testing with
// multi-profiles enabled.
class ProfileHelperTest : public InProcessBrowserTest {
 public:
  ProfileHelperTest() {
  }

 protected:
  void ActiveUserChanged(ProfileHelper* profile_helper,
                         const std::string& hash) {
    profile_helper->ActiveUserHashChanged(hash);
  }
};

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, ActiveUserProfileDir) {
  std::unique_ptr<ProfileHelper> profile_helper(
      ProfileHelper::CreateInstance());
  ActiveUserChanged(profile_helper.get(), kActiveUserHash);
  base::FilePath profile_dir = profile_helper->GetActiveUserProfileDir();
  std::string expected_dir;
  expected_dir.append(chrome::kProfileDirPrefix);
  expected_dir.append(kActiveUserHash);
  EXPECT_EQ(expected_dir, profile_dir.BaseName().value());
}

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, GetProfilePathByUserIdHash) {
  std::unique_ptr<ProfileHelper> profile_helper(
      ProfileHelper::CreateInstance());
  base::FilePath profile_path =
      profile_helper->GetProfilePathByUserIdHash(kActiveUserHash);
  base::FilePath expected_path = g_browser_process->profile_manager()->
      user_data_dir().Append(
          std::string(chrome::kProfileDirPrefix) + kActiveUserHash);
  EXPECT_EQ(expected_path, profile_path);
}

}  // namespace ash
