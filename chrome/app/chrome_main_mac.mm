// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_mac.h"

#import <Cocoa/Cocoa.h>

#include <string>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"

namespace {

// Checks if the system launched the alerts helper app via a notification
// action. If that's the case we want to gracefully exit the process as we can't
// handle the click this way. Instead we rely on the browser process to re-spawn
// the helper if it got killed unexpectedly.
bool IsAlertsHelperLaunchedViaNotificationAction() {
  // We allow the main Chrome app to be launched via a notification action. We
  // detect and log that to UMA by checking the passed in NSNotification in
  // -applicationDidFinishLaunching: (//chrome/browser/app_controller_mac.mm).
  if (!base::apple::IsBackgroundOnlyProcess()) {
    return false;
  }

  // If we have a process type then we were not launched by the system.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    return false;
  }

  base::FilePath path;
  if (!base::PathService::Get(base::FILE_EXE, &path)) {
    return false;
  }

  // Check if our executable name matches the helper app for notifications.
  std::string helper_name = path.BaseName().value();
  return base::EndsWith(helper_name, chrome::kMacHelperSuffixAlerts);
}

// Safe Exam Browser has been observed launching helper processes directly,
// without any command line arguments. The absence of required command line
// arguments is detected and rejected later on during process initialization,
// resulting in the process exiting with a `CHECK` failure. Safe Exam Browser
// is overwhelming the crash signature that many types of early process
// initialization failures are aggregated under. Explicitly detect this and exit
// cleanly instead. https://crbug.com/374353396
bool IsHelperAppLaunchedBySafeExamBrowser() {
  if (!base::apple::IsBackgroundOnlyProcess()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    return false;
  }

  std::string bundle_identifier = base::Environment::Create()
                                      ->GetVar("__CFBundleIdentifier")
                                      .value_or(std::string());

  return bundle_identifier == "org.safeexambrowser.SafeExamBrowser";
}

// Returns the NSBundle for the outer browser application, even when running
// inside the helper. In unbundled applications, such as tests, returns nil.
NSBundle* OuterAppBundle() {
  @autoreleasepool {
    if (!base::apple::AmIBundled()) {
      // If unbundled (as in a test), there's no app bundle.
      return nil;
    }

    if (!base::apple::IsBackgroundOnlyProcess()) {
      // Shortcut: in the browser process, just return the main app bundle.
      return NSBundle.mainBundle;
    }

    // From C.app/Contents/Frameworks/C.framework/Versions/1.2.3.4, go up five
    // steps to C.app.
    base::FilePath framework_path = chrome::GetFrameworkBundlePath();
    base::FilePath outer_app_dir =
        framework_path.DirName().DirName().DirName().DirName().DirName();
    NSString* outer_app_dir_ns = base::SysUTF8ToNSString(outer_app_dir.value());

    return [NSBundle bundleWithPath:outer_app_dir_ns];
  }
}

}  // namespace

void SetUpBundleOverrides() {
  @autoreleasepool {
    base::apple::SetOverrideFrameworkBundlePath(
        chrome::GetFrameworkBundlePath());

    NSBundle* base_bundle = OuterAppBundle();
    base::apple::SetOverrideOuterBundle(base_bundle);
    base::apple::SetBaseBundleIDOverride(
        base::SysNSStringToUTF8(base_bundle.bundleIdentifier));

    base::FilePath child_exe_path =
        chrome::GetFrameworkBundlePath().Append("Helpers").Append(
            chrome::kHelperProcessExecutablePath);

    // On the Mac, the child executable lives at a predefined location within
    // the app bundle's versioned directory.
    base::PathService::OverrideAndCreateIfNeeded(
        content::CHILD_PROCESS_EXE, child_exe_path, /*is_absolute=*/true,
        /*create=*/false);
  }
}

bool IsHelperAppLaunchedBySystemOrThirdPartyApplication() {
  // Gracefully exit if the system tried to launch the macOS notification helper
  // app when a user clicked on a notification.
  if (IsAlertsHelperLaunchedViaNotificationAction()) {
    return true;
  }

  // Gracefully exit if Safe Exam Browser tried to launch a helper app directly.
  if (IsHelperAppLaunchedBySafeExamBrowser()) {
    return true;
  }

  return false;
}
