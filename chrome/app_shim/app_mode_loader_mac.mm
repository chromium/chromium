// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, shortcuts can't have command-line arguments. Instead, produce small
// app bundles which locate the Chromium framework and load it, passing the
// appropriate data. This is the code for such an app bundle. It should be kept
// minimal and do as little work as possible (with as much work done on
// framework side as possible).

#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "base/mac/launch_services_util.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_chrome_locator.h"
#include "chrome/common/mac/app_mode_common.h"

namespace {

typedef int (*StartFun)(const app_mode::ChromeAppModeInfo*);

int LoadFrameworkAndStart(int argc, char** argv) {
  using base::SysNSStringToUTF8;
  using base::SysNSStringToUTF16;
  using base::mac::CFToNSCast;
  using base::mac::CFCastStrict;
  using base::mac::NSToCFCast;

  @autoreleasepool {
    // Get the current main bundle, i.e., that of the app loader that's running.
    NSBundle* app_bundle = [NSBundle mainBundle];
    CHECK(app_bundle) << "couldn't get loader bundle";

    // ** 1: Get path to outer Chrome bundle.
    // Get the bundle ID of the browser that created this app bundle.
    NSString* cr_bundle_id = base::mac::ObjCCast<NSString>(
        [app_bundle objectForInfoDictionaryKey:app_mode::kBrowserBundleIDKey]);
    CHECK(cr_bundle_id) << "couldn't get browser bundle ID";

    // First check if Chrome exists at the last known location.
    base::FilePath cr_bundle_path;
    NSString* cr_bundle_path_ns =
        [CFToNSCast(CFCastStrict<CFStringRef>(CFPreferencesCopyAppValue(
            NSToCFCast(app_mode::kLastRunAppBundlePathPrefsKey),
            NSToCFCast(cr_bundle_id)))) autorelease];
    cr_bundle_path = base::mac::NSStringToFilePath(cr_bundle_path_ns);
    bool found_bundle =
        !cr_bundle_path.empty() && base::DirectoryExists(cr_bundle_path);

    if (!found_bundle) {
      // If no such bundle path exists, try to search by bundle ID.
      if (!app_mode::FindBundleById(cr_bundle_id, &cr_bundle_path)) {
        // TODO(jeremy): Display UI to allow user to manually locate the Chrome
        // bundle.
        LOG(FATAL) << "Failed to locate bundle by identifier";
      }
    }

    // ** 2: Read the running Chrome version.
    // The user_data_dir for shims actually contains the app_data_path.
    // I.e. <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/
    base::FilePath app_data_dir = base::mac::NSStringToFilePath([app_bundle
        objectForInfoDictionaryKey:app_mode::kCrAppModeUserDataDirKey]);
    base::FilePath user_data_dir = app_data_dir.DirName().DirName().DirName();
    CHECK(!user_data_dir.empty());

    // If the version file does not exist, |cr_version_str| will be empty and
    // app_mode::GetChromeBundleInfo will default to the latest version.
    base::FilePath cr_version_str;
    base::ReadSymbolicLink(
        user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName),
        &cr_version_str);

    // If the version file does exist, it may have been left by a crashed Chrome
    // process. Ensure the process is still running.
    if (!cr_version_str.empty()) {
      NSArray* existing_chrome = [NSRunningApplication
          runningApplicationsWithBundleIdentifier:cr_bundle_id];
      if ([existing_chrome count] == 0)
        cr_version_str.clear();
    }

    // ** 3: Read information from the Chrome bundle.
    base::FilePath executable_path;
    base::FilePath framework_path;
    base::FilePath framework_dylib_path;
    if (!app_mode::GetChromeBundleInfo(cr_bundle_path, cr_version_str.value(),
                                       &executable_path, &framework_path,
                                       &framework_dylib_path)) {
      LOG(FATAL) << "Couldn't ready Chrome bundle info";
    }
    base::FilePath app_mode_bundle_path =
        base::mac::NSStringToFilePath([app_bundle bundlePath]);

    // ** 4: Read information from the Info.plist.
    // Read information about the this app shortcut from the Info.plist.
    // Don't check for null-ness on optional items.
    NSDictionary* info_plist = [app_bundle infoDictionary];
    CHECK(info_plist) << "couldn't get loader Info.plist";

    const std::string app_mode_id = SysNSStringToUTF8(
        [info_plist objectForKey:app_mode::kCrAppModeShortcutIDKey]);
    CHECK(app_mode_id.size()) << "couldn't get app shortcut ID";

    const std::string app_mode_name = SysNSStringToUTF8(
        [info_plist objectForKey:app_mode::kCrAppModeShortcutNameKey]);
    const std::string app_mode_url = SysNSStringToUTF8(
        [info_plist objectForKey:app_mode::kCrAppModeShortcutURLKey]);

    base::FilePath plist_user_data_dir = base::mac::NSStringToFilePath(
        [info_plist objectForKey:app_mode::kCrAppModeUserDataDirKey]);

    base::FilePath profile_dir = base::mac::NSStringToFilePath(
        [info_plist objectForKey:app_mode::kCrAppModeProfileDirKey]);

    // ** 5: Open the framework.
    StartFun ChromeAppModeStart = NULL;
    void* cr_dylib = dlopen(framework_dylib_path.value().c_str(), RTLD_LAZY);
    if (cr_dylib) {
      // Find the entry point.
      ChromeAppModeStart =
          (StartFun)dlsym(cr_dylib, APP_SHIM_ENTRY_POINT_NAME_STRING);
      if (!ChromeAppModeStart)
        LOG(ERROR) << "Couldn't get entry point: " << dlerror();
    } else {
      LOG(ERROR) << "Couldn't load framework: " << dlerror();
    }

    // ** 6: Fill in ChromeAppModeInfo and call into Chrome's framework.
    if (ChromeAppModeStart) {
      // Ensure that the strings pointed to by |info| outlive |info|.
      const std::string framework_path_utf8 = framework_path.AsUTF8Unsafe();
      const std::string cr_bundle_path_utf8 = cr_bundle_path.AsUTF8Unsafe();
      const std::string app_mode_bundle_path_utf8 =
          app_mode_bundle_path.AsUTF8Unsafe();
      const std::string plist_user_data_dir_utf8 =
          plist_user_data_dir.AsUTF8Unsafe();
      const std::string profile_dir_utf8 = profile_dir.AsUTF8Unsafe();
      app_mode::ChromeAppModeInfo info;
      info.argc = argc;
      info.argv = argv;
      info.chrome_framework_path = framework_path_utf8.c_str();
      info.chrome_outer_bundle_path = cr_bundle_path_utf8.c_str();
      info.app_mode_bundle_path = app_mode_bundle_path_utf8.c_str();
      info.app_mode_id = app_mode_id.c_str();
      info.app_mode_name = app_mode_name.c_str();
      info.app_mode_url = app_mode_url.c_str();
      info.user_data_dir = plist_user_data_dir_utf8.c_str();
      info.profile_dir = profile_dir_utf8.c_str();
      return ChromeAppModeStart(&info);
    }

    LOG(ERROR) << "Loading Chrome failed, launching Chrome with command line";
    base::CommandLine command_line(executable_path);
    // The user_data_dir from the plist is actually the app data dir.
    command_line.AppendSwitchPath(
        switches::kUserDataDir,
        plist_user_data_dir.DirName().DirName().DirName());
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            app_mode::kLaunchedByChromeProcessId)) {
      // Pass --app-shim-error to have Chrome rebuild this shim.
      // If Chrome has rebuilt this shim once already, then rebuilding doesn't
      // fix the problem, so don't try again.
      if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
              app_mode::kLaunchedAfterRebuild)) {
        command_line.AppendSwitchPath(app_mode::kAppShimError,
                                      app_mode_bundle_path);
      }
    } else {
      // If the shim was launched directly (instead of by Chrome), first ask
      // Chrome to launch the app. Chrome will launch the shim again, the same
      // error will occur and be handled above. This approach allows the app to
      // be started without blocking on fixing the shim and guarantees that the
      // profile is loaded when Chrome receives --app-shim-error.
      command_line.AppendSwitchPath(switches::kProfileDirectory, profile_dir);
      command_line.AppendSwitchASCII(switches::kAppId, app_mode_id);
    }
    // Launch the executable directly since base::mac::OpenApplicationWithPath
    // doesn't pass command line arguments if the application is already
    // running.
    if (!base::LaunchProcess(command_line, base::LaunchOptions()).IsValid()) {
      LOG(ERROR) << "Could not launch Chrome: "
                 << command_line.GetCommandLineString();
      return 1;
    }

    return 0;
  }
}

} // namespace

__attribute__((visibility("default")))
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Exit instead of returning to avoid the the removal of |main()| from stack
  // backtraces under tail call optimization.
  exit(LoadFrameworkAndStart(argc, argv));
}
