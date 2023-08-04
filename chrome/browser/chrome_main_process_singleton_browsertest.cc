// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/base/filename_util.h"

#if !BUILDFLAG(ENABLE_PROCESS_SINGLETON)
#error Not supported on this platform.
#endif

class ChromeMainTest : public InProcessBrowserTest {
 public:
  ChromeMainTest() {}

  void Relaunch(const base::CommandLine& new_command_line) {
    base::LaunchProcess(new_command_line, base::LaunchOptionsForTest());
  }

  Profile* CreateProfile(const base::FilePath& basename) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path =
        profile_manager->user_data_dir().Append(basename);
    return &profiles::testing::CreateProfileSync(profile_manager, profile_path);
  }

  // Gets the relaunch command line with the kProfileEmail switch.
  base::CommandLine GetCommandLineForRelaunchWithEmail(
      const std::string& email) {
    base::CommandLine command_line = GetCommandLineForRelaunch();
    command_line.AppendArg(
        base::StringPrintf("--profile-email=%s", email.c_str()));
    return command_line;
  }
};

// Make sure that the second invocation creates a new window.
IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunch) {
  Relaunch(GetCommandLineForRelaunch());
  ui_test_utils::WaitForBrowserToOpen();
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, ReuseBrowserInstanceWhenOpeningFile) {
  base::FilePath test_file_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("empty.html"));
  base::CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);
  Relaunch(new_command_line);
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  GURL url = net::FilePathToFileURL(test_file_path);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url, tab->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchWithIncognitoUrl) {
  // We should start with one normal window.
  ASSERT_EQ(1u, chrome::GetTabbedBrowserCount(browser()->profile()));

  // Run with --incognito switch and an URL specified.
  base::FilePath test_file_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("empty.html"));
  base::CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendSwitch(switches::kIncognito);
  new_command_line.AppendArgPath(test_file_path);

  Relaunch(new_command_line);

  // There should be one normal and one incognito window now.
  Relaunch(new_command_line);
  ui_test_utils::WaitForBrowserToOpen();
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1u, chrome::GetTabbedBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchFromIncognitoWithNormalUrl) {
  Profile* const profile = browser()->profile();

  // We should start with one normal window.
  ASSERT_EQ(1u, chrome::GetTabbedBrowserCount(profile));

  // Create an incognito window.
  chrome::NewIncognitoWindow(profile);

  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1u, chrome::GetTabbedBrowserCount(profile));

  // Close the first window.
  CloseBrowserSynchronously(browser());

  // There should only be the incognito window open now.
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(0u, chrome::GetTabbedBrowserCount(profile));

  // Run with just an URL specified, no --incognito switch.
  base::FilePath test_file_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("empty.html"));
  base::CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);
  Relaunch(new_command_line);
  ui_test_utils::WaitForBrowserToOpen();

  // There should be one normal and one incognito window now.
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1u, chrome::GetTabbedBrowserCount(profile));
}

// Multi-profile is not supported on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchWithProfileDir) {
  const base::FilePath kProfileDir(FILE_PATH_LITERAL("Other"));
  Profile* other_profile = CreateProfile(kProfileDir);
  ASSERT_TRUE(other_profile);

  // Pass the other profile path on the command line.
  base::CommandLine other_command_line = GetCommandLineForRelaunch();
  other_command_line.AppendSwitchPath(switches::kProfileDirectory, kProfileDir);
  size_t original_browser_count = chrome::GetTotalBrowserCount();
  Relaunch(other_command_line);
  Browser* other_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(other_browser);
  EXPECT_EQ(other_browser->profile(), other_profile);
  EXPECT_EQ(original_browser_count + 1, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchWithProfileEmail) {
  const base::FilePath kProfileDir1(FILE_PATH_LITERAL("Profile1"));
  const base::FilePath kProfileDir2(FILE_PATH_LITERAL("Profile2"));
  const std::string kProfileEmail1 = "example@gmail.com";
  // Unicode emails are supported.
  const std::string kProfileEmail2 =
      "\xe4\xbd\xa0\xe5\xa5\xbd\x40\x67\x6d\x61\x69\x6c\x2e\x63\x6f\x6d\x0a";
  ProfileAttributesStorage* storage =
      &g_browser_process->profile_manager()->GetProfileAttributesStorage();
  Profile* profile1 = CreateProfile(kProfileDir1);
  ASSERT_TRUE(profile1);
  storage->GetProfileAttributesWithPath(profile1->GetPath())
      ->SetAuthInfo("gaia_id_1", base::UTF8ToUTF16(kProfileEmail1),
                    /*is_consented_primary_account=*/false);
  Profile* profile2 = CreateProfile(kProfileDir2);
  ASSERT_TRUE(profile2);
  storage->GetProfileAttributesWithPath(profile2->GetPath())
      ->SetAuthInfo("gaia_id_2", base::UTF8ToUTF16(kProfileEmail2),
                    /*is_consented_primary_account=*/false);
  base::RunLoop run_loop;
  g_browser_process->FlushLocalStateAndReply(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  // Normal email.
  size_t original_browser_count = chrome::GetTotalBrowserCount();
  Relaunch(GetCommandLineForRelaunchWithEmail(kProfileEmail1));
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->profile(), profile1);
  EXPECT_EQ(original_browser_count + 1, chrome::GetTotalBrowserCount());
  // Non-ASCII email.
  Relaunch(GetCommandLineForRelaunchWithEmail(kProfileEmail2));
  new_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->profile(), profile2);
  EXPECT_EQ(original_browser_count + 2, chrome::GetTotalBrowserCount());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
