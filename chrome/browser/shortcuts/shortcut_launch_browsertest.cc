// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shortcuts {
namespace {

class ShortcutLaunchTestNotFoundProfile : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    command_line->AppendSwitchASCII(switches::kProfileDirectory, "NotFound");
    command_line->AppendSwitch(switches::kIgnoreProfileDirectoryIfNotExists);
    command_line->AppendArg(
        embedded_test_server()->GetURL("/title1.html").spec());
  }
};

IN_PROC_BROWSER_TEST_F(ShortcutLaunchTestNotFoundProfile, DefaultProfileUsed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(browser()->profile()->GetBaseName().value(),
            FILE_PATH_LITERAL("Default"));
}

using ShortcutLaunchTestFoundProfile = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ShortcutLaunchTestFoundProfile, SpecifiedProfileUsed) {
  const std::string kOtherProfile = "OtherProfile";
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path =
      profile_manager->user_data_dir().AppendASCII(kOtherProfile);
  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProfileDirectory, kOtherProfile);
  command_line.AppendSwitch(switches::kIgnoreProfileDirectoryIfNotExists);
  command_line.AppendArg(embedded_test_server()->GetURL("/title1.html").spec());

  // Note: `ProcessCommandLineAlreadyRunning` happens after
  // `GetStartupProfilePath` is called, the test has to call that directly.
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, /*cur_dir=*/{},
      GetStartupProfilePath(
          /*cur_dir=*/{}, command_line, /*ignore_profile_picker=*/false));

  Browser* browser = chrome::FindBrowserWithProfile(&other_profile);
  ASSERT_TRUE(browser);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/title1.html"));
}

IN_PROC_BROWSER_TEST_F(ShortcutLaunchTestFoundProfile,
                       ProfileMarkedForDeletion) {
  const std::string kOtherProfile = "OtherProfile";
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path =
      profile_manager->user_data_dir().AppendASCII(kOtherProfile);
  profiles::testing::CreateProfileSync(profile_manager, new_path);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProfileDirectory, kOtherProfile);
  command_line.AppendSwitch(switches::kIgnoreProfileDirectoryIfNotExists);
  command_line.AppendArg(embedded_test_server()->GetURL("/title1.html").spec());

  MarkProfileDirectoryForDeletion(new_path);

  // Note: `ProcessCommandLineAlreadyRunning` happens after
  // `GetStartupProfilePath` is called, the test has to call that directly.
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, /*cur_dir=*/{},
      GetStartupProfilePath(
          /*cur_dir=*/{}, command_line, /*ignore_profile_picker=*/false));

  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_EQ(browser->profile()->GetBaseName().value(),
            FILE_PATH_LITERAL("Default"));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/title1.html"));
}

}  // namespace
}  // namespace shortcuts
