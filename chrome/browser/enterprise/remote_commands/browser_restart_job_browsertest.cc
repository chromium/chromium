// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/browser_restart_job.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace enterprise_commands {

namespace {

const policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

enterprise_management::RemoteCommand CreateCommandProto(
    base::Time command_issued_time) {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_RESTART);
  int64_t age_of_command =
      (base::Time::Now() - command_issued_time).InMilliseconds();
  command_proto.set_age_of_command(age_of_command);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_payload("{}");
  return command_proto;
}

void InitJob(BrowserRestartJob* job,
             const enterprise_management::RemoteCommand& command_proto) {
  ASSERT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                        enterprise_management::SignedData{}));
  ASSERT_EQ(kUniqueID, job->unique_id());
  ASSERT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());
}

std::unique_ptr<BrowserRestartJob> CreateJob(
    const enterprise_management::RemoteCommand& command_proto) {
  auto job = std::make_unique<BrowserRestartJob>();
  InitJob(job.get(), command_proto);
  return job;
}

}  // namespace

class BrowserRestartJobTest : public InProcessBrowserTest {
 public:
  BrowserRestartJobTest()
      : relaunch_chrome_override_(
            base::BindRepeating(&BrowserRestartJobTest::OnRestart,
                                base::Unretained(this))) {}

  void TearDownOnMainThread() override {
    ASSERT_NE(std::nullopt, relaunch_expected_)
        << "Every test should call ExpectRelaunch(), to specify whether "
           "it expects a browser relaunch or not.";
  }

  ~BrowserRestartJobTest() override {
    EXPECT_EQ(relaunch_expected_.value(), did_relaunch_);
  }

  base::FilePath GetTimestampFilePath() {
    return browser()->profile()->GetPath().AppendASCII("timestamp.txt");
  }

  void ExpectRelaunch(bool relaunch_expected) {
    ASSERT_EQ(std::nullopt, relaunch_expected_)
        << "Cannot call ExpectRelaunch() multiple times per test.";
    relaunch_expected_ = relaunch_expected;
  }

 private:
  bool OnRestart(const base::CommandLine& command_line) {
    did_relaunch_ = true;
    return true;
  }

  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_;
  std::optional<bool> relaunch_expected_ = std::nullopt;
  bool did_relaunch_ = false;
};

// Tests that the browser restart is triggered if the command is issued
// after the browser has launched.
IN_PROC_BROWSER_TEST_F(BrowserRestartJobTest,
                       PRE_RestartsBrowserIfCommandIssuedAfterLaunch) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::Time command_issued_time = base::Time::Now();
  // Write `command_issued_time` to a file in the user-data directory, so the
  // non-PRE test can use the same value.
  base::TimeDelta command_issued_time_since_epoch =
      command_issued_time.ToDeltaSinceWindowsEpoch();
  ASSERT_TRUE(base::WriteFile(
      GetTimestampFilePath(),
      base::NumberToString(command_issued_time_since_epoch.InMicroseconds())));

  // TODO(nicolaso): Not doing this causes the non-PRE test to flake, because
  // Process::CreationTime() has a few milliseconds of coarseness on Linux and
  // Windows. I *suspect* that's a Spectre/timing attack mitigation at the OS
  // level, since the OS clock should be way more accurate than that...
  //
  // This hack can be removed when we use PrefService to store the command
  // unique_id.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(5));
  run_loop.Run();

  auto job = CreateJob(CreateCommandProto(command_issued_time));
  bool done = false;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        job->Run(base::Time::Now(), base::TimeTicks::Now(),
                 base::BindLambdaForTesting([&] { done = true; }));
      }));
  ui_test_utils::WaitForBrowserToClose();

  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(done);
  ExpectRelaunch(true);
}

// Tests that the command completes successfully, after the browser was
// restarted by the PRE_ test.
IN_PROC_BROWSER_TEST_F(BrowserRestartJobTest,
                       RestartsBrowserIfCommandIssuedAfterLaunch) {
  // Read and parse `command_issued_time` from the file written in the PRE_
  // test.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string timestamp_string;
  ASSERT_TRUE(
      base::ReadFileToString(GetTimestampFilePath(), &timestamp_string));
  int64_t timestamp_microseconds;
  ASSERT_TRUE(base::StringToInt64(timestamp_string, &timestamp_microseconds));
  base::Time command_issued_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(timestamp_microseconds));

  auto job = CreateJob(CreateCommandProto(command_issued_time));
  bool done = false;
  base::RunLoop run_loop;
  job->Run(base::Time::Now(), base::TimeTicks::Now(),
           base::BindLambdaForTesting([&] {
             done = true;
             run_loop.Quit();
           }));
  run_loop.Run();

  EXPECT_TRUE(done);
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  ExpectRelaunch(false);
}

// Tests that the browser restart is not triggered, and the command is marked as
// complete, if the command is issued before the browser launches.
IN_PROC_BROWSER_TEST_F(BrowserRestartJobTest,
                       DoesNotRestartBrowserIfCommandIssuedBeforeLaunch) {
  auto job =
      CreateJob(CreateCommandProto(base::Time::Now() - base::Minutes(5)));
  bool done = false;
  base::RunLoop run_loop;
  job->Run(base::Time::Now(), base::TimeTicks::Now(),
           base::BindLambdaForTesting([&] {
             done = true;
             run_loop.Quit();
           }));
  run_loop.Run();

  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(done);
  ExpectRelaunch(false);
}

// Tests that the restart skips onbeforeunload handlers.
IN_PROC_BROWSER_TEST_F(BrowserRestartJobTest, IgnoresOnBeforeUnload) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(tab,
                              "window.addEventListener('beforeunload',"
                              "(event) => { event.returnValue = 'Foo'; });"));
  content::PrepContentsForBeforeUnloadTest(tab);

  auto job = CreateJob(CreateCommandProto(base::Time::Now()));
  bool done = false;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        job->Run(base::Time::Now(), base::TimeTicks::Now(),
                 base::BindLambdaForTesting([&] { done = true; }));
      }));
  ui_test_utils::WaitForBrowserToClose();

  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(done);
  ExpectRelaunch(true);
}

}  // namespace enterprise_commands
