// Copyright 2013 The Chromium Authors
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
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kActiveUserHash[] = "01234567890";
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
  ProfileHelper* profile_helper = ProfileHelper::Get();
  ActiveUserChanged(profile_helper, kActiveUserHash);
  base::FilePath profile_dir = profile_helper->GetActiveUserProfileDir();
  std::string expected_dir =
      BrowserContextHelper::GetUserBrowserContextDirName(kActiveUserHash);
  EXPECT_EQ(expected_dir, profile_dir.BaseName().value());
}

}  // namespace ash
