// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/purge_stale_screen_capture_permission.h"

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "base/mac/bundle_locations.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "ui/base/cocoa/permissions_utils.h"

namespace chrome {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

bool ResetTCCScreenCaptureForBundleID(NSString* bundle_id) {
  if (!bundle_id.length) {
    return false;
  }
  std::vector<std::string> argv = {"/usr/bin/tccutil", "reset", "ScreenCapture",
                                   bundle_id.UTF8String};

  base::LaunchOptions launch_options;
  base::Process p = base::LaunchProcess(argv, launch_options);
  int status;
  return p.WaitForExit(&status) && status == 0;
}

bool AttemptPurgeStaleScreenCapturePermission() {
  if (ui::IsScreenCaptureAllowed()) {
    // We have access, no stale records exist.
    return true;
  }

  // We don't have access. This could be because a stale record exists or there
  // are no records. Reset the permission. This will clear out any potentially
  // stale records. If there are no records a reset is harmless. The loop is
  // paranoia about the exec failing for some reason. Retry once for good
  // measure.
  NSString* bundle_identifier = base::mac::MainBundle().bundleIdentifier;
  for (int i = 0; i < 2; ++i) {
    if (ResetTCCScreenCaptureForBundleID(bundle_identifier)) {
      // The stale record has been purged.
      return true;
    }
  }

  // A stale record could still exist. Let the caller know.
  return false;
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void PurgeStaleScreenCapturePermission() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // TCC doesn’t supervise screen capture until macOS 10.15.
  if (@available(macOS 10.15, *)) {
    static NSString* const kPreferenceKeyBase =
        @"PurgeStaleScreenCapturePermissionSuccessEarly2022";
    static NSString* const kPreferenceKeyAttemptsSuffix = @"Attempts";
    static NSString* const kPreferenceKeySuccessSuffix = @"Success";
    NSString* success_key = [kPreferenceKeyBase
        stringByAppendingString:kPreferenceKeySuccessSuffix];
    NSString* attempts_key = [kPreferenceKeyBase
        stringByAppendingString:kPreferenceKeyAttemptsSuffix];

    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    if ([user_defaults boolForKey:success_key]) {
      return;
    }

    const int kAttemptsMax = 3;
    int attempts_value = [user_defaults integerForKey:attempts_key];
    if ((attempts_value >= kAttemptsMax)) {
      return;
    }
    [user_defaults setInteger:++attempts_value forKey:attempts_key];
    base::ScopedAllowBlocking allow_blocking;
    if (AttemptPurgeStaleScreenCapturePermission()) {
      // Future startups will now return from PurgeStaleScreenCapturePermission
      // early.
      [user_defaults setBool:YES forKey:success_key];
    }
  }

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace chrome
