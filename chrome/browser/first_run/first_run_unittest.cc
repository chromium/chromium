// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/initial_preferences.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/mock_callback.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace first_run {

namespace {

base::FilePath GetTestDataPath(const std::string& test_name) {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .AppendASCII("first_run")
      .AppendASCII(test_name);
}

base::FilePath GetSentinelFilePath() {
  return base::PathService::CheckedGet(chrome::DIR_USER_DATA)
      .Append(chrome::kFirstRunSentinel);
}

}  // namespace

class FirstRunTest : public testing::Test {
 protected:
  void TearDown() override {
    first_run::ResetCachedSentinelDataForTesting();
    Test::TearDown();
  }

 private:
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
};

TEST_F(FirstRunTest, SetupInitialPrefsFromInstallPrefs_NoVariationsSeed) {
  installer::InitialPreferences install_prefs("{ }");
  EXPECT_TRUE(install_prefs.initial_dictionary().empty());

  EXPECT_TRUE(install_prefs.GetCompressedVariationsSeed().empty());
  EXPECT_TRUE(install_prefs.GetVariationsSeedSignature().empty());
}

TEST_F(FirstRunTest,
       SetupInitialPrefsFromInstallPrefs_VariationsSeedSignature) {
  installer::InitialPreferences install_prefs(
      "{\"variations_compressed_seed\":\"xyz\","
      " \"variations_seed_signature\":\"abc\"}");
  EXPECT_EQ(2U, install_prefs.initial_dictionary().size());

  EXPECT_EQ("xyz", install_prefs.GetCompressedVariationsSeed());
  EXPECT_EQ("abc", install_prefs.GetVariationsSeedSignature());
  // Variations prefs should have been extracted (removed) from the dictionary.
  EXPECT_TRUE(install_prefs.initial_dictionary().empty());
}

// No switches and no sentinel present. This is the standard case for first run.
TEST_F(FirstRunTest, DetermineFirstRunState_FirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// Force switch is present, overriding both sentinel and suppress switch.
TEST_F(FirstRunTest, DetermineFirstRunState_ForceSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(true, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// No switches, but sentinel present. This is not a first run.
TEST_F(FirstRunTest, DetermineFirstRunState_NotFirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, false, false);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

// Suppress switch is present, overriding sentinel state.
TEST_F(FirstRunTest, DetermineFirstRunState_SuppressSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);

  result = internal::DetermineFirstRunState(true, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_Created) {
  base::HistogramTester histogram_tester;
  first_run::CreateSentinelIfNeeded();
  histogram_tester.ExpectUniqueSample(
      "FirstRun.Sentinel.Created",
      startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);

  // Gets the creation time of the first run sentinel.
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(GetSentinelFilePath(), &info));

  EXPECT_EQ(info.creation_time, first_run::GetFirstRunSentinelCreationTime());
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_NotCreated) {
  base::File::Info info;
  ASSERT_FALSE(base::GetFileInfo(GetSentinelFilePath(), &info));

  EXPECT_EQ(
      0,
      first_run::GetFirstRunSentinelCreationTime().InSecondsFSinceUnixEpoch());
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // We are still considered in the first run, but we'll attempt a creation
    // even if the file exists.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }

  first_run::ResetCachedSentinelDataForTesting();

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_FALSE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // The file already exists, and we identified that we are not in the first
    // run, the creation is not needed.
    histogram_tester.ExpectTotalCount("FirstRun.Sentinel.Created", 0);
  }
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded_DoneEvenIfForced) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceFirstRun);

  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // While the first run state is forced, we'll always attempt to create the
    // sentinel.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }

  first_run::ResetCachedSentinelDataForTesting();

  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(base::PathExists(GetSentinelFilePath()));
    EXPECT_TRUE(IsChromeFirstRun());

    first_run::CreateSentinelIfNeeded();

    // While the first run state is forced, we'll always attempt to create the
    // sentinel.
    histogram_tester.ExpectUniqueSample(
        "FirstRun.Sentinel.Created",
        startup_metric_utils::FirstRunSentinelCreationResult::kFilePathExists,
        1);
  }
}

TEST_F(FirstRunTest, CreateSentinelIfNeeded_SkippedIfSuppressed) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);

  first_run::CreateSentinelIfNeeded();

  histogram_tester.ExpectTotalCount("FirstRun.Sentinel.Created", 0);
  EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));
  EXPECT_FALSE(IsChromeFirstRun());
}

#if BUILDFLAG(IS_POSIX)  // This test relies on Posix file permissions.
TEST_F(FirstRunTest, CreateSentinelIfNeeded_FileSystemError) {
  base::HistogramTester histogram_tester;

  // Make the user data dir read-only so the sentinel can't be written.
  // Note: the test fixture registers an override to a temp dir for the
  // scope of each test, the below is not as destructive as it seems.
  auto path = base::PathService::CheckedGet(chrome::DIR_USER_DATA);
  ASSERT_TRUE(SetPosixFilePermissions(
      path, DirectoryExists(path) ? (S_IRUSR | S_IXUSR) : S_IRUSR));

  first_run::CreateSentinelIfNeeded();

  histogram_tester.ExpectUniqueSample(
      "FirstRun.Sentinel.Created",
      startup_metric_utils::FirstRunSentinelCreationResult::kFileSystemError,
      1);
  EXPECT_FALSE(base::PathExists(GetSentinelFilePath()));

  EXPECT_TRUE(IsChromeFirstRun());  // This is still a first run.
}
#endif

// This test, and the one below, require customizing the path that the initial
// prefs code will search. On non-Mac platforms that path is derived from
// PathService, but on macOS it instead comes from the system analog of
// PathService (NSSearchPathForDirectoriesInDomains), which we don't have an
// analogous scoped override for.
//
// TODO(ellyjones): Add a scoped override for
// NSSearchPathForDirectoriesInDomains, then re-enable these on macOS.

#if BUILDFLAG(IS_MAC)
#define MAYBE_InitialPrefsUsedIfReadable DISABLED_InitialPrefsUsedIfReadable
#else
#define MAYBE_InitialPrefsUsedIfReadable InitialPrefsUsedIfReadable
#endif

TEST_F(FirstRunTest, MAYBE_InitialPrefsUsedIfReadable) {
  base::ScopedPathOverride override(base::DIR_EXE, GetTestDataPath("initial"));
  std::unique_ptr<installer::InitialPreferences> prefs =
      first_run::LoadInitialPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->GetFirstRunTabs()[0], "https://www.chromium.org/initial");
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent \
  DISABLED_LegacyInitialPrefsUsedIfNewFileIsNotPresent
#else
#define MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent \
  LegacyInitialPrefsUsedIfNewFileIsNotPresent
#endif

TEST_F(FirstRunTest, MAYBE_LegacyInitialPrefsUsedIfNewFileIsNotPresent) {
  base::ScopedPathOverride override(base::DIR_EXE, GetTestDataPath("legacy"));
  std::unique_ptr<installer::InitialPreferences> prefs =
      first_run::LoadInitialPrefs();

  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->GetFirstRunTabs()[0], "https://www.chromium.org/legacy");
}

#if BUILDFLAG(IS_WIN)
TEST_F(FirstRunTest, GetRelaunchCommandLine_RestartLastSession) {
  base::CommandLine cl(base::FilePath(FILE_PATH_LITERAL("chrome.exe")));
  cl.AppendSwitch(switches::kApp);
  cl.AppendSwitchASCII("custom-switch", "custom-value");
  cl.AppendArg("https://example.com");

  base::CommandLine relaunch_cl = upgrade_util::GetRelaunchCommandLine(
      cl, browser_shutdown::RestartMode::kRestartLastSession);

  EXPECT_TRUE(relaunch_cl.HasSwitch(switches::kRestart));
  EXPECT_FALSE(relaunch_cl.HasSwitch(switches::kApp));
  EXPECT_TRUE(relaunch_cl.HasSwitch("custom-switch"));
  EXPECT_EQ(relaunch_cl.GetSwitchValueASCII("custom-switch"), "custom-value");
  EXPECT_TRUE(relaunch_cl.GetArgs().empty());
}

TEST_F(FirstRunTest, GetRelaunchCommandLine_RestartInBackground) {
  base::CommandLine cl(base::FilePath(FILE_PATH_LITERAL("chrome.exe")));
  cl.AppendSwitch(switches::kApp);
  cl.AppendSwitchASCII("custom-switch", "custom-value");
  cl.AppendArg("https://example.com");

  base::CommandLine relaunch_cl = upgrade_util::GetRelaunchCommandLine(
      cl, browser_shutdown::RestartMode::kRestartInBackground);

  EXPECT_FALSE(relaunch_cl.HasSwitch(switches::kRestart));
  EXPECT_TRUE(relaunch_cl.HasSwitch(switches::kNoStartupWindow));
  EXPECT_FALSE(relaunch_cl.HasSwitch(switches::kApp));
  EXPECT_TRUE(relaunch_cl.HasSwitch("custom-switch"));
  EXPECT_EQ(relaunch_cl.GetSwitchValueASCII("custom-switch"), "custom-value");
  ASSERT_EQ(relaunch_cl.GetArgs().size(), 1u);
  EXPECT_EQ(relaunch_cl.GetArgs()[0],
            app_launch_prefetch::GetPrefetchSwitch(
                app_launch_prefetch::SubprocessType::kBrowserBackground));
}

TEST_F(FirstRunTest, GetRelaunchCommandLine_RestartThisSession) {
  base::CommandLine cl(base::FilePath(FILE_PATH_LITERAL("chrome.exe")));
  cl.AppendSwitchASCII("custom-switch", "custom-value");
  cl.AppendArg("https://example.com");

  base::CommandLine relaunch_cl = upgrade_util::GetRelaunchCommandLine(
      cl, browser_shutdown::RestartMode::kRestartThisSession);

  EXPECT_TRUE(relaunch_cl.HasSwitch(switches::kRestart));
  EXPECT_FALSE(relaunch_cl.HasSwitch(switches::kNoStartupWindow));
  EXPECT_TRUE(relaunch_cl.HasSwitch("custom-switch"));
  EXPECT_EQ(relaunch_cl.GetSwitchValueASCII("custom-switch"), "custom-value");
  ASSERT_EQ(relaunch_cl.GetArgs().size(), 1u);
  EXPECT_EQ(relaunch_cl.GetArgs()[0], FILE_PATH_LITERAL("https://example.com"));
}

TEST_F(FirstRunTest, RelaunchChromeBrowser_WaitForParentFalseRemovesSwitch) {
  base::CommandLine cl(base::FilePath(FILE_PATH_LITERAL("chrome.exe")));
  cl.AppendSwitchASCII(switches::kWaitForParentHandle, "1234");

  base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback> callback;
  EXPECT_CALL(callback, Run(::testing::ResultOf(
                            [](const base::CommandLine& command_line) {
                              return command_line.HasSwitch(
                                  switches::kWaitForParentHandle);
                            },
                            ::testing::IsFalse())))
      .WillOnce(::testing::Return(true));
  upgrade_util::ScopedRelaunchChromeBrowserOverride capture_cl(callback.Get());
  upgrade_util::RelaunchChromeBrowser(cl, /*force_breakaway_from_job=*/false,
                                      /*wait_for_parent=*/false);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace first_run
