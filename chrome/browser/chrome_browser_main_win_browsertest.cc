// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

// Returns the number of child processes the browser has created so far,
// counting a process as soon as its host exists, which happens when the launch
// is requested rather than when the child finishes connecting.
int CountChildProcessHosts() {
  int count = 0;
  for (content::BrowserChildProcessHostIterator it; !it.Done(); ++it) {
    ++count;
  }
  for (content::RenderProcessHost::iterator it =
           content::RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    ++count;
  }
  return count;
}

// Runs a closure from the startup phase that performs the in-use update swap.
class StartupPhaseObserver : public ChromeBrowserMainExtraParts {
 public:
  explicit StartupPhaseObserver(base::OnceClosure post_create_threads)
      : post_create_threads_(std::move(post_create_threads)) {}

  // ChromeBrowserMainExtraParts:
  void PostCreateThreads() override { std::move(post_create_threads_).Run(); }

 private:
  base::OnceClosure post_create_threads_;
};

}  // namespace

class ChromeBrowserMainWinTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    InProcessBrowserTest::SetUpLocalStatePrefService(local_state);
    if (GetTestPreCount() > 0) {
      // Clear the migration version and time prefs set by
      // InProcessBrowserTest::SetUpLocalStatePrefService.
      local_state->ClearPref(prefs::kShortcutMigrationVersion);
      local_state->ClearPref(prefs::kShortcutMigrationTime);
    } else {
      // Set the version back to kLastVersionNeedingMigration and a recent
      // timestamp so `ShortcutsAreMigratedOnce` will verify that it's not
      // migrated again.
      local_state->SetString(prefs::kShortcutMigrationVersion, "156.0.8069.0");
      initial_migration_time_ = base::Time::Now() - base::Days(1);
      local_state->SetTime(prefs::kShortcutMigrationTime,
                           initial_migration_time_);
    }
  }

 protected:
  base::Time initial_migration_time_;
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinTest, PRE_ShortcutsAreMigratedOnce) {
  // Wait for all startup tasks to run.
  content::RunAllTasksUntilIdle();

  // Confirm that shortcuts were migrated.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(local_state->GetString(prefs::kShortcutMigrationVersion),
            version_info::GetVersionNumber());
  EXPECT_FALSE(local_state->GetTime(prefs::kShortcutMigrationTime).is_null());
}

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinTest, ShortcutsAreMigratedOnce) {
  content::RunAllTasksUntilIdle();

  // Confirm that shortcuts weren't migrated when marked as having last been
  // migrated in kLastVersionNeedingMigration+ and within the re-check interval.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(local_state->GetString(prefs::kShortcutMigrationVersion),
            "156.0.8069.0");
  EXPECT_EQ(local_state->GetTime(prefs::kShortcutMigrationTime),
            initial_migration_time_);
}

class ChromeBrowserMainWinPeriodicMigrationTest : public InProcessBrowserTest {
 public:
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    InProcessBrowserTest::SetUpLocalStatePrefService(local_state);
    time_before_test_ = base::Time::Now();
    local_state->SetString(prefs::kShortcutMigrationVersion, "156.0.8069.0");
    local_state->SetTime(prefs::kShortcutMigrationTime,
                         time_before_test_ - base::Days(31));
  }

 protected:
  base::Time time_before_test_;
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinPeriodicMigrationTest,
                       ShortcutsAreMigratedAfterInterval) {
  content::RunAllTasksUntilIdle();

  // Confirm that shortcuts were migrated again because the last migration time
  // was older than the 30-day interval.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(local_state->GetString(prefs::kShortcutMigrationVersion),
            version_info::GetVersionNumber());
  EXPECT_GE(local_state->GetTime(prefs::kShortcutMigrationTime),
            time_before_test_);
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

// Verifies that the in-use update swap performed by upgrade_util::
// DoUpgradeTasks() happens before the browser creates any child process.
//
// ChromeBrowserMainPartsWin::PostCreateThreads() runs the swap first, then
// defers to ChromeBrowserMainParts::PostCreateThreads(), which runs the
// ChromeBrowserMainExtraParts below. That is still ahead of
// BrowserMainLoop::PostCreateThreadsImpl(), where content first requests the
// GPU process, so it is an accurate marker for "the swap is done".
class ChromeBrowserMainWinUpdateSwapTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<StartupPhaseObserver>(base::BindOnce(
            &ChromeBrowserMainWinUpdateSwapTest::OnUpdateSwapCompleted,
            base::Unretained(this))));
  }

  bool swap_completed() const { return swap_completed_; }
  int child_process_hosts_at_swap() const {
    return child_process_hosts_at_swap_;
  }

 private:
  void OnUpdateSwapCompleted() {
    swap_completed_ = true;
    child_process_hosts_at_swap_ = CountChildProcessHosts();
  }

  bool swap_completed_ = false;
  int child_process_hosts_at_swap_ = 0;
};

IN_PROC_BROWSER_TEST_F(ChromeBrowserMainWinUpdateSwapTest,
                       NoChildProcessLaunchesBeforeUpdateSwap) {
  ASSERT_TRUE(swap_completed());

  // A child process launched before the swap could map the newly swapped-in
  // executable while the browser is still running the old one, which the
  // sandbox rejects when it validates the child against the broker's image.
  EXPECT_EQ(0, child_process_hosts_at_swap());

  // Ensure the check above cannot pass vacuously: by the time the test body
  // runs the browser has a tab, so child processes must exist.
  content::RunAllTasksUntilIdle();
  EXPECT_GT(CountChildProcessHosts(), 0);
}
