// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/process/process.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/test/browser_process_backgrounded_observer_mac.h"
#include "content/public/test/browser_test.h"
#include "gpu/config/gpu_finch_features.h"

namespace {

// Ensures the current app is focused.
void FocusCurrentApp() {
  NSRunningApplication* app = NSRunningApplication.currentApplication;
  [app activateWithOptions:NSApplicationActivateAllWindows |
                           NSApplicationActivateIgnoringOtherApps];
}

// Ensures the current app is no longer focused. Returns true on success.
bool UnfocusCurrentApp() {
  NSArray* running_applications =
      [[NSWorkspace sharedWorkspace] runningApplications];
  for (NSRunningApplication* app in running_applications) {
    if ([app.bundleIdentifier.lowercaseString
            isEqualToString:@"com.apple.finder"]) {
      [app activateWithOptions:NSApplicationActivateAllWindows |
                               NSApplicationActivateIgnoringOtherApps];
      return true;
    }
  }
  return false;
}

// Returns true if the process is `TASK_BACKGROUND_APPLICATION`.
bool IsProcessBackgrounded(base::ProcessId pid) {
  base::Process process = base::Process::Open(pid);
  EXPECT_TRUE(process.IsValid());
  return process.IsProcessBackgrounded(
      content::BrowserChildProcessHost::GetPortProvider());
}

// Returns the pid of the GPU process.
int GetGPUProcessId() {
  content::BrowserChildProcessHostIterator it(content::PROCESS_TYPE_GPU);
  EXPECT_FALSE(it.Done());
  int pid = it.GetData().GetProcess().Pid();
  // Ensure there was only one GPU process.
  ++it;
  EXPECT_TRUE(it.Done());
  return pid;
}

}  // namespace

class BrowserChildProcessBackgroundedBridgeTest
    : public InProcessBrowserTest,
      public base::PortProvider::Observer,
      public content::ProcessBackgroundedObserver {
 public:
  BrowserChildProcessBackgroundedBridgeTest()
      : content::ProcessBackgroundedObserver(
            base::BindRepeating(&BrowserChildProcessBackgroundedBridgeTest::
                                    OnProcessBackgroundedChanged,
                                base::Unretained(this))) {}
  ~BrowserChildProcessBackgroundedBridgeTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAdjustGpuProcessPriority);
    InProcessBrowserTest::SetUp();
  }

  // Waits until the port for the GPU process is available.
  void WaitForGpuPort() {
    base::PortProvider* port_provider =
        content::BrowserChildProcessHost::GetPortProvider();

    // Immediately return if the GPU port is already available.
    if (port_provider->TaskForPid(GetGPUProcessId()) != 0) {
      return;
    }

    content::BrowserChildProcessHost::GetPortProvider()->AddObserver(this);
    base::RunLoop run_loop;
    on_task_port_received_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Waits until the backgrounded state of the GPU process changes and returns
  // that new state through the out parameter.
  void WaitUntilBackgroundedChanged(bool* backgrounded) {
    base::RunLoop run_loop;
    on_backgrounded_changed_callback_ = base::BindOnce(
        [](base::OnceClosure quit_closure, bool* backgrounded, bool new_value) {
          *backgrounded = new_value;
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), backgrounded);
    ;
    run_loop.Run();
  }

  void OnProcessBackgroundedChanged(int process_id, bool backgrounded) {
    // A task to set the process' foregrounded state was posted to the process
    // launcher task runner. Do a roundtrip to ensure that task has had the time
    // to run.
    content::GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&BrowserChildProcessBackgroundedBridgeTest::
                           OnProcessBackgroundedActuallyChanged,
                       weak_ptr_factory_.GetWeakPtr(), process_id,
                       backgrounded));
  }

  void OnProcessBackgroundedActuallyChanged(int process_id, bool backgrounded) {
    if (process_id != GetGPUProcessId()) {
      return;
    }

    if (on_backgrounded_changed_callback_) {
      std::move(on_backgrounded_changed_callback_).Run(backgrounded);
    }
  }

 private:
  void OnReceivedTaskPort(base::ProcessHandle process_handle) override {
    if (process_handle != GetGPUProcessId()) {
      return;
    }

    content::BrowserChildProcessHost::GetPortProvider()->RemoveObserver(this);
    std::move(on_task_port_received_callback_).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::OnceClosure on_task_port_received_callback_;
  base::OnceCallback<void(bool)> on_backgrounded_changed_callback_;

  base::WeakPtrFactory<BrowserChildProcessBackgroundedBridgeTest>
      weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       FocusAndUnfocus) {
  // Wait until we receive the port for the GPU process.
  WaitForGpuPort();

  {
    // Unfocusing Chrome should cause the GPU process to be backgrounded.
    ASSERT_TRUE(UnfocusCurrentApp());
    const bool kExpectedBackgrounded = true;
    bool backgrounded = !kExpectedBackgrounded;
    WaitUntilBackgroundedChanged(&backgrounded);
    ASSERT_EQ(backgrounded, kExpectedBackgrounded);
    ASSERT_EQ(IsProcessBackgrounded(GetGPUProcessId()), kExpectedBackgrounded);
  }

  {
    // Refocusing Chrome should now cause it to be foregrounded.
    FocusCurrentApp();
    const bool kExpectedBackgrounded = false;
    bool backgrounded = !kExpectedBackgrounded;
    WaitUntilBackgroundedChanged(&backgrounded);
    ASSERT_EQ(backgrounded, kExpectedBackgrounded);
    ASSERT_EQ(IsProcessBackgrounded(GetGPUProcessId()), kExpectedBackgrounded);
  }
}
