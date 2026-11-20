// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/app_shim/app_shim_application.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/remote_cocoa/app_shim/features.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#import "ui/base/test/scoped_fake_nswindow_focus.h"

namespace {

constexpr char kFeatureOnByDefaultName[] = "AppShimOnByDefault";
BASE_FEATURE(kFeatureOnByDefault,
             kFeatureOnByDefaultName,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kFeatureOffByDefaultName[] = "AppShimOffByDefault";
BASE_FEATURE(kFeatureOffByDefault,
             kFeatureOffByDefaultName,
             base::FEATURE_DISABLED_BY_DEFAULT);

void PersistFeatureState(
    const variations::VariationsCommandLine& feature_state) {
  feature_state.WriteToFile(base::PathService::CheckedGet(chrome::DIR_USER_DATA)
                                .Append(app_mode::kFeatureStateFileName));
}

}  // namespace

class AppShimControllerTest : public testing::Test {
 private:
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
};

TEST_F(AppShimControllerTest, EarlyAccessFeatureAllowList) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AppShimController::PreInitFeatureState(command_line);

  // Reset crash-on-early-access flag.
  base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();

  // Should not be able to access arbitrary features without getting early
  // access errors.
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_EQ(&kFeatureOnByDefault,
            base::FeatureList::GetEarlyAccessedFeatureForTesting());
  base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
}

TEST_F(AppShimControllerTest, FeatureStateFromCommandLine) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 kFeatureOffByDefaultName);
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 kFeatureOnByDefaultName);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FeatureStateFromFeatureFile) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  feature_state.disable_features = kFeatureOnByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FeatureStateFromFeatureFileAndCommandLine) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  feature_state.disable_features = kFeatureOnByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 kFeatureOffByDefaultName);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest,
       FeatureStateFromFeatureFileIsIgnoredWhenLaunchedByChrome) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(app_mode::kLaunchedByChromeProcessId);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FinalizeFeatureState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread io_thread("CrAppShimIO");
  io_thread.Start();
  auto io_thread_runner = io_thread.task_runner();

  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 kFeatureOffByDefaultName);
  AppShimController::PreInitFeatureState(command_line);
  base::ScopedClosureRunner reset_crash_keys =
      variations::InitCrashKeysForTesting();

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = "";
  feature_state.disable_features = base::JoinString(
      {kFeatureOnByDefaultName, kFeatureOffByDefaultName}, ",");
  AppShimController::FinalizeFeatureState(std::move(feature_state),
                                          io_thread_runner);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());

  // Verify that AppShimController did not leave the io thread blocked.
  base::RunLoop run_loop;
  io_thread_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(AppShimControllerTest, FinalizeFeatureStateWithFieldTrials) {
  static constexpr char kTrialName[] = "TrialName";
  static constexpr char kTrialGroup1Name[] = "Group1";
  static constexpr char kTrialGroup2Name[] = "Group2";
  static constexpr char kParam1Name[] = "param1";
  static constexpr char kParam2Name[] = "param2";

  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread io_thread("CrAppShimIO");
  io_thread.Start();
  auto io_thread_runner = io_thread.task_runner();

  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  static const base::FeatureParam<int> feature_param1{&kFeatureOffByDefault,
                                                      kParam1Name, 0};
  static const base::FeatureParam<int> feature_param2{&kFeatureOffByDefault,
                                                      kParam2Name, 0};

  variations::VariationsCommandLine feature_state_in_file;
  feature_state_in_file.enable_features =
      base::StringPrintf("%s<%s", kFeatureOffByDefaultName, kTrialName);
  feature_state_in_file.disable_features = "";
  feature_state_in_file.field_trial_states =
      base::StringPrintf("%s/%s", kTrialName, kTrialGroup1Name);
  feature_state_in_file.field_trial_params = base::StringPrintf(
      "%s.%s:%s/13,%s.%s:%s/5", kTrialName, kTrialGroup1Name, kParam1Name,
      kTrialName, kTrialGroup2Name, kParam1Name);
  PersistFeatureState(feature_state_in_file);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      variations::switches::kForceFieldTrialParams,
      base::StringPrintf("%s.%s:%s/42", kTrialName, kTrialGroup1Name,
                         kParam2Name));
  AppShimController::PreInitFeatureState(command_line);
  base::ScopedClosureRunner reset_crash_keys =
      variations::InitCrashKeysForTesting();

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
  // With both command line and feature state file data the values on the
  // command line should take priority.
  EXPECT_EQ(0, feature_param1.Get());
  EXPECT_EQ(42, feature_param2.Get());

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features =
      base::StringPrintf("%s<%s", kFeatureOffByDefaultName, kTrialName);
  feature_state.disable_features = "";
  feature_state.field_trial_states =
      base::StringPrintf("%s/%s", kTrialName, kTrialGroup2Name);
  feature_state.field_trial_params = base::StringPrintf(
      "%s.%s:%s/2", kTrialName, kTrialGroup2Name, kParam1Name);
  AppShimController::FinalizeFeatureState(feature_state, io_thread_runner);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
  EXPECT_EQ(2, feature_param1.Get());
  EXPECT_EQ(0, feature_param2.Get());
}

@interface AppShimTestFakeWindow : NSWindow
@property(strong, nonatomic) NSEvent* lastEvent;
@property(assign, nonatomic) BOOL wasHandlingSendEvent;
@end

@implementation AppShimTestFakeWindow

@synthesize lastEvent = _lastEvent;
@synthesize wasHandlingSendEvent = _wasHandlingSendEvent;

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (void)sendEvent:(NSEvent*)event {
  self.lastEvent = event;
  self.wasHandlingSendEvent =
      [base::apple::ObjCCastStrict<CrApplication>(NSApp) isHandlingSendEvent];
}

@end

MULTIPROCESS_TEST_MAIN(AppShimApplicationSendEventMain) {
  [AppShimApplication sharedApplication];
  CHECK([NSApp isKindOfClass:[AppShimApplication class]]);

  NSEvent* cmd_a_key_up = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                           location:NSZeroPoint
                                      modifierFlags:NSEventModifierFlagCommand
                                          timestamp:0
                                       windowNumber:0
                                            context:nil
                                         characters:@"a"
                        charactersIgnoringModifiers:@"a"
                                          isARepeat:NO
                                            keyCode:0];

  // With no key window during early feature access, Cmd+KeyUp is a safe no-op
  // and does not trigger an early feature access violation.
  {
    base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);
    base::test::ScopedFeatureList clear_feature_list;
    clear_feature_list.InitWithNullFeatureAndFieldTrialLists();
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    AppShimController::PreInitFeatureState(command_line);
    base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();

    CHECK_EQ(nil, NSApp.keyWindow);
    [NSApp sendEvent:cmd_a_key_up];
    CHECK(!base::FeatureList::GetEarlyAccessedFeatureForTesting());
  }

  ui::test::ScopedFakeNSWindowFocus fake_focus;
  AppShimTestFakeWindow* fake_window = [[AppShimTestFakeWindow alloc] init];
  fake_window.releasedWhenClosed = NO;
  [fake_window makeKeyWindow];
  CHECK_EQ(fake_window, NSApp.keyWindow);

  // When enabled (default), Cmd+KeyUp is forwarded to the key window and
  // ScopedSendingEvent marks NSApp as handling sendEvent:.
  {
    base::test::ScopedFeatureList enable_feature(
        remote_cocoa::features::kForwardCmdKeyUpEventsToWindow);
    [NSApp sendEvent:cmd_a_key_up];
    CHECK_EQ(cmd_a_key_up, fake_window.lastEvent);
    CHECK(fake_window.wasHandlingSendEvent);
    CHECK(![base::apple::ObjCCastStrict<CrApplication>(NSApp)
        isHandlingSendEvent]);
  }

  // When the killswitch is disabled, Cmd+KeyUp is not forwarded to the key
  // window.
  fake_window.lastEvent = nil;
  {
    base::test::ScopedFeatureList disable_feature;
    disable_feature.InitAndDisableFeature(
        remote_cocoa::features::kForwardCmdKeyUpEventsToWindow);
    [NSApp sendEvent:cmd_a_key_up];
    CHECK_EQ(nil, fake_window.lastEvent);
  }

  [fake_window close];
  return 0;
}

TEST_F(AppShimControllerTest, AppShimApplicationCmdKeyUpForwardedToWindow) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitch(switches::kDoNotCreateNSAppForTests);
  base::Process child = base::SpawnMultiProcessTestChild(
      "AppShimApplicationSendEventMain", command_line, base::LaunchOptions());
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

namespace {

constexpr char kTestUserDataDirSwitch[] = "app-shim-test-user-data-dir";
constexpr char kTestBundlePathSwitch[] = "app-shim-test-bundle-path";
constexpr char kTestReadyFileSwitch[] = "app-shim-test-ready-file";
constexpr char kTestTerminatedFileSwitch[] = "app-shim-test-terminated-file";
constexpr char kTestFatalFileSwitch[] = "app-shim-test-fatal-file";
constexpr int kExpectedFatalExitCode = 42;

class ScopedChildProcess {
 public:
  explicit ScopedChildProcess(base::Process process)
      : process_(std::move(process)) {}
  ~ScopedChildProcess() { TerminateAndReap(); }

  bool IsValid() const { return process_.IsValid(); }
  base::ProcessId Pid() const { return process_.Pid(); }

  bool WaitForExit(int* exit_code) {
    bool exited = base::WaitForMultiprocessTestChildExit(
        process_, TestTimeouts::action_timeout(), exit_code);
    if (exited) {
      process_ = base::Process();
    }
    return exited;
  }

  bool TerminateAndReap() {
    if (!process_.IsValid()) {
      return true;
    }
    bool terminated = process_.Terminate(/*exit_code=*/0, /*wait=*/true);
    process_ = base::Process();
    return terminated;
  }

 private:
  base::Process process_;
};

bool WaitUntil(base::FunctionRef<bool()> condition) {
  const base::TimeTicks deadline =
      base::TimeTicks::Now() + TestTimeouts::action_timeout();
  while (!condition()) {
    if (base::TimeTicks::Now() >= deadline) {
      return false;
    }
    base::PlatformThread::Sleep(base::Milliseconds(10));
  }
  return true;
}

base::CommandLine ChildCommandLine() {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitch(switches::kDoNotCreateNSAppForTests);
  return command_line;
}

base::FilePath CreateMinimalAppBundle(const base::FilePath& dir) {
  base::FilePath bundle_path = dir.Append("TestApp.app");
  base::FilePath contents_dir = bundle_path.Append("Contents");
  CHECK(base::CreateDirectory(contents_dir));
  constexpr std::string_view kInfoPlist =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key>
  <string>org.chromium.test.appshim</string>
  <key>CrBundleIdentifier</key>
  <string>org.chromium.test.nonexistent.browser</string>
</dict>
</plist>
)";
  CHECK(base::WriteFile(contents_dir.Append("Info.plist"), kInfoPlist));
  return bundle_path;
}

class PollReadyTestDelegate : public AppShimController::TestDelegate {
 public:
  explicit PollReadyTestDelegate(base::FilePath ready_file)
      : ready_file_(std::move(ready_file)) {
    AppShimController::SetDelegateForTesting(this);
  }
  ~PollReadyTestDelegate() override {
    AppShimController::SetDelegateForTesting(nullptr);
  }

  void PopulateChromeCommandLine(base::CommandLine& command_line) override {}

  void OnPollForChromeReady() override {
    if (!written_) {
      written_ = true;
      CHECK(base::WriteFile(ready_file_, "1"));
    }
  }

 private:
  const base::FilePath ready_file_;
  bool written_ = false;
};

}  // namespace

MULTIPROCESS_TEST_MAIN(AppShimNoOpChildMain) {
  return 0;
}

MULTIPROCESS_TEST_MAIN(AppShimSleepingChildMain) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestReadyFileSwitch)) {
    CHECK(base::WriteFile(
        command_line->GetSwitchValuePath(kTestReadyFileSwitch), "1"));
  } else {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];
  }
  base::PlatformThread::Sleep(TestTimeouts::action_max_timeout());
  return 0;
}

MULTIPROCESS_TEST_MAIN(AppShimLaunchedByChromeMain) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::unique_ptr<logging::ScopedLogAssertHandler> fatal_handler;
  if (command_line->HasSwitch(kTestFatalFileSwitch)) {
    base::FilePath fatal_file =
        command_line->GetSwitchValuePath(kTestFatalFileSwitch);
    fatal_handler =
        std::make_unique<logging::ScopedLogAssertHandler>(base::BindRepeating(
            [](base::FilePath fatal_file, const char* /*file*/, int /*line*/,
               std::string_view message, std::string_view /*stack_trace*/) {
              base::WriteFile(fatal_file, message);
              base::Process::TerminateCurrentProcessImmediately(
                  kExpectedFatalExitCode);
            },
            fatal_file));
  }

  if (command_line->HasSwitch(kTestBundlePathSwitch)) {
    base::apple::SetOverrideMainBundlePath(
        command_line->GetSwitchValuePath(kTestBundlePathSwitch));
  }

  base::ScopedPathOverride user_data_dir_override(
      chrome::DIR_USER_DATA,
      command_line->GetSwitchValuePath(kTestUserDataDirSwitch));
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();
  AppShimController::PreInitFeatureState(*command_line);

  [AppShimApplication sharedApplication];
  CHECK([NSApp isKindOfClass:[AppShimApplication class]]);
  [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI,
                                                    /*is_main_thread=*/true);
  AppShimController::SetDisableNotificationServiceForTesting(true);

  std::optional<PollReadyTestDelegate> poll_ready_delegate;
  if (command_line->HasSwitch(kTestReadyFileSwitch)) {
    poll_ready_delegate.emplace(
        command_line->GetSwitchValuePath(kTestReadyFileSwitch));
  }

  AppShimController::Params params;
  params.user_data_dir = base::PathService::CheckedGet(chrome::DIR_USER_DATA);
  params.app_id = "test_app_id";
  params.app_name = u"Test App";
  AppShimController controller(params);

  id will_terminate_observer = nil;
  if (command_line->HasSwitch(kTestTerminatedFileSwitch)) {
    base::FilePath terminated_file =
        command_line->GetSwitchValuePath(kTestTerminatedFileSwitch);
    will_terminate_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationWillTerminateNotification
                    object:NSApp
                     queue:nil
                usingBlock:^(NSNotification*) {
                  CHECK(base::WriteFile(terminated_file, "1"));
                }];
  }

  base::RunLoop().Run();
  (void)will_terminate_observer;
  return 1;
}

TEST_F(AppShimControllerTest,
       LaunchedByChromeExitsCleanlyWhenChromePidDoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath terminated_file = temp_dir.GetPath().Append("terminated");

  // Spawn and reap a short-lived process to obtain a guaranteed dead PID.
  ScopedChildProcess dead_child(base::SpawnMultiProcessTestChild(
      "AppShimNoOpChildMain", ChildCommandLine(), base::LaunchOptions()));
  ASSERT_TRUE(dead_child.IsValid());
  const base::ProcessId dead_pid = dead_child.Pid();
  int dead_rv = -1;
  ASSERT_TRUE(dead_child.WaitForExit(&dead_rv));
  ASSERT_EQ(0, dead_rv);

  base::CommandLine shim_cmd = ChildCommandLine();
  shim_cmd.AppendSwitchPath(kTestUserDataDirSwitch,
                            temp_dir.GetPath().Append("user_data"));
  shim_cmd.AppendSwitchASCII(app_mode::kLaunchedByChromeProcessId,
                             base::NumberToString(dead_pid));
  shim_cmd.AppendSwitchPath(kTestTerminatedFileSwitch, terminated_file);
  ScopedChildProcess shim(base::SpawnMultiProcessTestChild(
      "AppShimLaunchedByChromeMain", shim_cmd, base::LaunchOptions()));
  ASSERT_TRUE(shim.IsValid());
  int rv = -1;
  ASSERT_TRUE(shim.WaitForExit(&rv));
  EXPECT_EQ(0, rv);
  EXPECT_TRUE(base::PathExists(terminated_file));
}

TEST_F(AppShimControllerTest,
       LaunchedByChromeExitsCleanlyWhenChromeTerminatesWhilePolling) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath bundle_path = CreateMinimalAppBundle(temp_dir.GetPath());
  base::FilePath ready_file = temp_dir.GetPath().Append("ready");
  base::FilePath terminated_file = temp_dir.GetPath().Append("terminated");

  // Spawn a fake Chrome NSApplication process without an AppShimListener Mach
  // server so the shim enters PollForChromeReady().
  ScopedChildProcess fake_chrome(base::SpawnMultiProcessTestChild(
      "AppShimSleepingChildMain", ChildCommandLine(), base::LaunchOptions()));
  ASSERT_TRUE(fake_chrome.IsValid());
  const base::ProcessId fake_chrome_pid = fake_chrome.Pid();
  ASSERT_TRUE(WaitUntil([&] {
    @autoreleasepool {
      NSRunningApplication* app = [NSRunningApplication
          runningApplicationWithProcessIdentifier:fake_chrome_pid];
      return app != nil && !app.terminated;
    }
  }));

  base::CommandLine shim_cmd = ChildCommandLine();
  shim_cmd.AppendSwitchPath(kTestUserDataDirSwitch,
                            temp_dir.GetPath().Append("user_data"));
  shim_cmd.AppendSwitchASCII(app_mode::kLaunchedByChromeProcessId,
                             base::NumberToString(fake_chrome_pid));
  shim_cmd.AppendSwitchPath(kTestBundlePathSwitch, bundle_path);
  shim_cmd.AppendSwitchPath(kTestReadyFileSwitch, ready_file);
  shim_cmd.AppendSwitchPath(kTestTerminatedFileSwitch, terminated_file);
  ScopedChildProcess shim(base::SpawnMultiProcessTestChild(
      "AppShimLaunchedByChromeMain", shim_cmd, base::LaunchOptions()));
  ASSERT_TRUE(shim.IsValid());

  ASSERT_TRUE(WaitUntil([&] { return base::PathExists(ready_file); }));
  ASSERT_TRUE(fake_chrome.TerminateAndReap());

  int rv = -1;
  ASSERT_TRUE(shim.WaitForExit(&rv));
  EXPECT_EQ(0, rv);
  EXPECT_TRUE(base::PathExists(terminated_file));
}

TEST_F(AppShimControllerTest,
       LaunchedByChromeFatalWhenLiveProcessNotFoundByAppKit) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath ready_file = temp_dir.GetPath().Append("ready");
  base::FilePath fatal_file = temp_dir.GetPath().Append("fatal");

  // Spawn a live process without NSApplication so AppKit cannot find it,
  // verifying live-PID lookup failures (crbug.com/40261534) remain fatal.
  base::CommandLine live_cmd = ChildCommandLine();
  live_cmd.AppendSwitchPath(kTestReadyFileSwitch, ready_file);
  ScopedChildProcess live_child(base::SpawnMultiProcessTestChild(
      "AppShimSleepingChildMain", live_cmd, base::LaunchOptions()));
  ASSERT_TRUE(live_child.IsValid());
  ASSERT_TRUE(WaitUntil([&] { return base::PathExists(ready_file); }));

  base::CommandLine shim_cmd = ChildCommandLine();
  shim_cmd.AppendSwitchPath(kTestUserDataDirSwitch,
                            temp_dir.GetPath().Append("user_data"));
  shim_cmd.AppendSwitchASCII(app_mode::kLaunchedByChromeProcessId,
                             base::NumberToString(live_child.Pid()));
  shim_cmd.AppendSwitchPath(kTestFatalFileSwitch, fatal_file);
  ScopedChildProcess shim(base::SpawnMultiProcessTestChild(
      "AppShimLaunchedByChromeMain", shim_cmd, base::LaunchOptions()));
  ASSERT_TRUE(shim.IsValid());
  int rv = -1;
  ASSERT_TRUE(shim.WaitForExit(&rv));
  EXPECT_EQ(kExpectedFatalExitCode, rv);
  std::string fatal_message;
  ASSERT_TRUE(base::ReadFileToString(fatal_file, &fatal_message));
  EXPECT_NE(fatal_message.find(base::StringPrintf(
                "Failed to open process with PID: %d", live_child.Pid())),
            std::string::npos)
      << fatal_message;
}
