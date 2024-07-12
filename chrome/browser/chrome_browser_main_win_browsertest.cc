// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

class ChromeBrowserMainWinTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    InProcessBrowserTest::SetUpLocalStatePrefService(local_state);
    if (GetTestPreCount() > 0) {
      // Clear the migration version pref set by
      // InProcessBrowserTest::SetUpLocalStatePrefService.
      local_state->ClearPref(prefs::kShortcutMigrationVersion);
    } else {
      // Set the version back to kLastVersionNeedingMigration and
      // `ShortcutsAreMigratedOnce` will verify that it's not migrated again.
      local_state->SetString(prefs::kShortcutMigrationVersion, "86.0.4231.0");
    }
  }
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinTest, PRE_ShortcutsAreMigratedOnce) {
  // Wait for all startup tasks to run.
  content::RunAllTasksUntilIdle();

  // Confirm that shortcuts were migrated.
  const std::string last_version_migrated =
      g_browser_process->local_state()->GetString(
          prefs::kShortcutMigrationVersion);
  EXPECT_EQ(last_version_migrated, version_info::GetVersionNumber());
}

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinTest, ShortcutsAreMigratedOnce) {
  content::RunAllTasksUntilIdle();

  // Confirm that shortcuts weren't migrated when marked as having last been
  // migrated in kLastVersionNeedingMigration+.
  const std::string last_version_migrated =
      g_browser_process->local_state()->GetString(
          prefs::kShortcutMigrationVersion);
  EXPECT_EQ(last_version_migrated, "86.0.4231.0");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

class OsUpdaterEnabledTest : public ChromeBrowserMainWinTest,
                             public ::testing::WithParamInterface<bool> {
 protected:
  OsUpdaterEnabledTest() = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.emplace_back(features::kRegisterOsUpdateHandlerWin);
    } else {
      disabled_features.emplace_back(features::kRegisterOsUpdateHandlerWin);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, OsUpdaterEnabledTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(OsUpdaterEnabledTest, OsUpdateHelper) {
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(g_browser_process->local_state()->GetBoolean(
                prefs::kOsUpdateHandlerEnabled),
            GetParam());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
