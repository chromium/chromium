// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace first_run {

class FirstRunInternalPosixTest : public InProcessBrowserTest {
 protected:
  FirstRunInternalPosixTest() = default;

  FirstRunInternalPosixTest(const FirstRunInternalPosixTest&) = delete;
  FirstRunInternalPosixTest& operator=(const FirstRunInternalPosixTest&) =
      delete;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // For Chrome, the presence of a Local State file should not influence whether
  // the first run dialog is shown. See crbug.com/1221483.
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    const base::FilePath local_state_file =
        user_data_dir.Append(chrome::kLocalStateFilename);
    const std::string empty_prefs = "{}";
    base::WriteFile(local_state_file, empty_prefs.data());
    return true;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // While Chromium uses ForcedShowDialogState::kForceShown, Chrome uses the
    // default, ForcedShowDialogState::kNotForced, to exercise Chrome-specific
    // behavior. See ShouldShowFirstRunDialog().
    internal::ForceFirstRunDialogShownForTesting(true);
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

    // The modal dialog will spawn and spin a nested RunLoop when
    // content::BrowserTestBase::SetUp() invokes content::ContentMain().
    // BrowserTestBase sets ContentMainParams::ui_task before this, but the
    // ui_task isn't actually Run() until after the dialog is spawned in
    // ChromeBrowserMainParts::PreMainMessageLoopRunImpl(). Instead, try to
    // inspect state by posting a task to run in that nested RunLoop. There's no
    // MessageLoop to enqueue a task on yet or anything else sensible that would
    // allow us to hook in a task. So use a testing-only Closure.
    GetBeforeShowFirstRunDialogHookForTesting() = base::BindOnce(
        &FirstRunInternalPosixTest::SetupNestedTask, base::Unretained(this));
    EXPECT_FALSE(inspected_state_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    EXPECT_TRUE(inspected_state_);
    // The test closure should have run. But clear the global in case it hasn't.
    EXPECT_FALSE(GetBeforeShowFirstRunDialogHookForTesting());
    GetBeforeShowFirstRunDialogHookForTesting().Reset();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

 private:
  // A task run immediately before first_run::DoPostImportPlatformSpecificTasks
  // shows the first-run dialog.
  void SetupNestedTask() {
    EXPECT_TRUE(base::SequencedTaskRunner::GetCurrentDefault());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FirstRunInternalPosixTest::InspectState,
                                  base::Unretained(this)));
  }

  // A task queued up to run once the first-run dialog starts pumping messages
  // in its modal loop.
  void InspectState() {
    // Send a signal to myself. This should post a task for the next run loop
    // iteration to set browser_shutdown::IsTryingToQuit(), and interrupt the
    // RunLoop.
    raise(SIGINT);
    inspected_state_ = true;
  }

  bool inspected_state_ = false;
};

// Test the first run flow for showing the modal dialog that surfaces the first
// run dialog. Ensure browser startup safely handles a signal while the modal
// RunLoop is running.
// TODO(crbug.com/338037494): Flaky on Linux ASan.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_HandleSigint DISABLED_HandleSigint
#else
#define MAYBE_HandleSigint HandleSigint
#endif  //  BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(FirstRunInternalPosixTest, MAYBE_HandleSigint) {
  // Never reached. The above SIGINT should prevent the main message loop
  // (and the browser test hooking it) from running.
  ADD_FAILURE() << "Should never be called";
}

}  // namespace first_run
