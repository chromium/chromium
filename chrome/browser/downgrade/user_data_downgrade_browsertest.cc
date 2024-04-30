// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace downgrade {

// A basic test fixture for User Data downgrade tests that writes a test file
// and a "Last Version" file into User Data in the pre-relaunch case. The former
// is used to validate move-and-delete processing on downgrade, while the latter
// is to simulate a previous run of a higher version of the browser. The fixture
// is expected to be used in a PRE_ and a regular test, with IsPreTest used to
// distinguish these cases at runtime.
class UserDataDowngradeBrowserTestBase : public InProcessBrowserTest {
 public:
  UserDataDowngradeBrowserTestBase(const UserDataDowngradeBrowserTestBase&) =
      delete;
  UserDataDowngradeBrowserTestBase& operator=(
      const UserDataDowngradeBrowserTestBase&) = delete;

 protected:
  // Returns true if the PRE_ test is running, meaning that the test is in the
  // "before relaunch" stage.
  static bool IsPreTest() {
    const std::string_view test_name(
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    return test_name.find("PRE_") != std::string_view::npos;
  }

  // Returns the next Chrome milestone version.
  static std::string GetNextChromeVersion() {
    return base::NumberToString(version_info::GetVersion().components()[0] + 1);
  }

  UserDataDowngradeBrowserTestBase()
      : root_key_(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                    : HKEY_CURRENT_USER) {}

  // Returns the registry hive into which the browser is registered.
  HKEY root_key() const { return root_key_; }

  // Returns the path to the User Data dir.
  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  // Returns the destination path to which User Data may be or was moved.
  const base::FilePath& moved_user_data_dir() const {
    return moved_user_data_dir_;
  }

  // Returns the path to some generated file in User Data.
  const base::FilePath& other_file() const { return other_file_; }

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(root_key_));
    ASSERT_TRUE(base::win::RegKey(
                    root_key_, install_static::GetClientStateKeyPath().c_str(),
                    KEY_SET_VALUE | KEY_WOW64_32KEY)
                    .Valid());
    InProcessBrowserTest::SetUp();
  }

  bool SetUpUserDataDirectory() override {
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
      return false;
    other_file_ = user_data_dir_.Append(FILE_PATH_LITERAL("Other File"));
    moved_user_data_dir_ = user_data_dir_.Append(user_data_dir_.BaseName())
                               .AddExtension(kDowngradeDeleteSuffix);
    if (IsPreTest()) {
      // Create some "other file" to be convinced that stuff is moved.
      if (!base::WriteFile(other_file_, "data"))
        return false;
      // Pretend that a higher version of Chrome previously wrote User Data.
      const std::string last_version = GetNextChromeVersion();
      base::WriteFile(user_data_dir_.Append(kDowngradeLastVersionFile),
                      last_version);
    }
    return true;
  }

 private:
  // The registry hive into which the browser is/will be registered.
  const HKEY root_key_;

  // The location of User Data.
  base::FilePath user_data_dir_;

  // The location into which the contents of User Data may be moved in case of
  // downgrade.
  base::FilePath moved_user_data_dir_;

  // The path to an arbitrary file in the user data dir that will be present
  // only when a reset does not take place.
  base::FilePath other_file_;

  registry_util::RegistryOverrideManager registry_override_manager_;
};

// A gMock matcher that is satisfied when its argument is a command line
// containing a given switch.
MATCHER_P(HasSwitch, switch_name, "") {
  return arg.HasSwitch(switch_name);
}

// A test fixture that triggers a downgrade, expects a relaunch, and verifies
// that User Data was moved and then subsequently deleted.
class UserDataDowngradeBrowserCopyAndCleanTest
    : public UserDataDowngradeBrowserTestBase {
 public:
  UserDataDowngradeBrowserCopyAndCleanTest(
      const UserDataDowngradeBrowserCopyAndCleanTest&) = delete;
  UserDataDowngradeBrowserCopyAndCleanTest& operator=(
      const UserDataDowngradeBrowserCopyAndCleanTest&) = delete;

 protected:
  using ParentClass = UserDataDowngradeBrowserTestBase;

  UserDataDowngradeBrowserCopyAndCleanTest() = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    if (ParentClass::IsPreTest()) {
      // Pretend that a downgrade was performed via the installer.
      ASSERT_NO_FATAL_FAILURE(SetDowngradeVersion(GetNextChromeVersion()));

      // Expect a browser relaunch late in browser shutdown.
      mock_relaunch_callback_ = std::make_unique<::testing::StrictMock<
          base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>>();
      EXPECT_CALL(*mock_relaunch_callback_,
                  Run(HasSwitch(switches::kUserDataMigrated)));
      relaunch_chrome_override_ =
          std::make_unique<upgrade_util::ScopedRelaunchChromeBrowserOverride>(
              mock_relaunch_callback_->Get());

      // Expect that browser startup short-circuits into a relaunch.
      set_expected_exit_code(chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH);

      // Prepare to check histograms during the restart.
      histogram_tester_ = std::make_unique<base::HistogramTester>();
    } else {
      // Verify the contents of the renamed user data directory.
      ASSERT_TRUE(base::DirectoryExists(moved_user_data_dir()));
      EXPECT_TRUE(base::PathExists(
          moved_user_data_dir().Append(other_file().BaseName())));
      EXPECT_EQ(GetNextChromeVersion(),
                GetLastVersion(moved_user_data_dir())->GetString());
    }
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    ParentClass::CreatedBrowserMainParts(parts);
    if (!ParentClass::IsPreTest()) {
      // Ensure that the after-startup task to delete User Data has a chance to
      // run.
      static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
          std::make_unique<
              RunAllPendingTasksPostMainMessageLoopRunExtraParts>());
    }
  }

  void SetUpOnMainThread() override {
    // This is never reached in the pre test due to the relaunch.
    ASSERT_FALSE(ParentClass::IsPreTest());
    ParentClass::SetUpOnMainThread();
  }

  void TearDownInProcessBrowserTestFixture() override {
    if (!ParentClass::IsPreTest()) {
      // Verify the renamed user data directory has been deleted.
      EXPECT_FALSE(base::DirectoryExists(moved_user_data_dir()));
    }
  }

 private:
  // DeleteMovedUserDataSoon() is called after the test is run, and will post a
  // task to perform the actual deletion. Make sure this task gets a chance to
  // run, so that the check in TearDownInProcessBrowserTestFixture() works.
  class RunAllPendingTasksPostMainMessageLoopRunExtraParts
      : public ChromeBrowserMainExtraParts {
   public:
    void PostMainMessageLoopRun() override { content::RunAllTasksUntilIdle(); }
  };

  // Writes |downgrade_version| into the DowngradeVersion value in ClientState
  // so that the browser believes that a downgrade was driven by an
  // administrator rather than an accident of fate.
  void SetDowngradeVersion(std::string_view downgrade_version) {
    ASSERT_EQ(base::win::RegKey(root_key(),
                                install_static::GetClientStateKeyPath().c_str(),
                                KEY_SET_VALUE | KEY_WOW64_32KEY)
                  .WriteValue(L"DowngradeVersion",
                              base::ASCIIToWide(downgrade_version).c_str()),
              ERROR_SUCCESS);
  }

  std::unique_ptr<
      base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>
      mock_relaunch_callback_;
  std::unique_ptr<upgrade_util::ScopedRelaunchChromeBrowserOverride>
      relaunch_chrome_override_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Verify the user data directory has been renamed and created again after
// downgrade.
IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserCopyAndCleanTest, PRE_Test) {}

IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserCopyAndCleanTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(chrome::kChromeVersion,
            GetLastVersion(user_data_dir())->GetString());
  EXPECT_FALSE(base::PathExists(other_file()));
}

// A test fixture that launches the browser twice for a non-administrator
// driven downgrade and ensures that User Data is not moved aside and deleted.
class UserDataDowngradeBrowserNoResetTest
    : public UserDataDowngradeBrowserTestBase {
 public:
  UserDataDowngradeBrowserNoResetTest(
      const UserDataDowngradeBrowserNoResetTest&) = delete;
  UserDataDowngradeBrowserNoResetTest& operator=(
      const UserDataDowngradeBrowserNoResetTest&) = delete;

 protected:
  UserDataDowngradeBrowserNoResetTest() = default;
};

// Verify the user data directory will not be reset without downgrade.
IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserNoResetTest, PRE_Test) {}

// TODO(crbug.com/40925550): Re-enable this test
IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserNoResetTest, DISABLED_Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_EQ(chrome::kChromeVersion,
            GetLastVersion(user_data_dir())->GetString());
  EXPECT_TRUE(base::PathExists(other_file()));
}

}  // namespace downgrade
