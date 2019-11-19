// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_mac.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "components/metal_util/test_shader.h"

namespace {

// Allow up to 1 minute for shader compilation. This is much longer than the
// GPU process limit of 6 seconds.
constexpr base::TimeDelta kTestShaderCompileTimeout =
    base::TimeDelta::FromMinutes(1);

void TestShaderCallback(const base::TimeTicks& start_time,
                        metal::TestShaderResult result) {
  base::TimeDelta delta;
  switch (result) {
    case metal::TestShaderResult::kNotAttempted:
    case metal::TestShaderResult::kFailed:
      // Don't include data if no Metal device was created (e.g, due to hardware
      // or macOS version reasons).
      return;
    case metal::TestShaderResult::kTimedOut:
      // Use a single histogram for both "how long did compile take" and "did
      // compile complete in 1 minute" by pushing timeouts into the maximum
      // bucket of UMA_HISTOGRAM_MEDIUM_TIMES.
      delta = base::TimeDelta::FromMinutes(3);
      break;
    case metal::TestShaderResult::kSucceeded:
      delta = base::TimeTicks::Now() - start_time;
      break;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Browser.Metal.TestShaderCompileTime", delta);
}

// Launch a test metal shader compile to see how long it takes to complete (if
// it ever completes).
// https://crbug.com/974219
void TestShaderLaunch() {
  metal::TestShader(metal::kTestShaderSeedBrowserTimer,
                    base::BindOnce(TestShaderCallback, base::TimeTicks::Now()),
                    kTestShaderCompileTimeout);
}

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart() {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::StartTearDown() {
  app_shim_listener_ = nullptr;
}

void BrowserProcessPlatformPart::AttemptExit(bool try_to_quit_application) {
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  //
  // This function is called for two types of attempted exits: URL requests
  // (chrome://quit or chrome://restart), and a keyboard menu invocations of
  // command-Q. (Interestingly, selecting the Quit command with the mouse don't
  // come down this code path at all.) URL requests to exit have
  // |try_to_quit_application| set to true; keyboard menu invocations have it
  // set to false.

  if (!try_to_quit_application) {
    // A keyboard menu invocation.
    AppController* app_controller =
        base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
    if (![app_controller runConfirmQuitPanel])
      return;
  }

  chrome_browser_application_mac::Terminate();
}

void BrowserProcessPlatformPart::PreMainMessageLoopRun() {
  // AppShimListener can not simply be reset, otherwise destroying the old
  // domain socket will cause the just-created socket to be unlinked.
  DCHECK(!app_shim_listener_.get());
  app_shim_listener_ = new AppShimListener;

  // Launch a test Metal shader compile once the run loop starts.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(TestShaderLaunch));
}

AppShimListener* BrowserProcessPlatformPart::app_shim_listener() {
  return app_shim_listener_.get();
}
