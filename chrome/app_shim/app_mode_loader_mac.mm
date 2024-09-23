// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, shortcuts can't have command-line arguments. Instead, produce small
// app bundles which locate the Chromium framework and load it, passing the
// appropriate data. This is the code for such an app bundle. It should be kept
// minimal and do as little work as possible (with as much work done on
// framework side as possible).

#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

#include "base/allocator/early_zone_registration_apple.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_chrome_locator.h"
#include "chrome/common/mac/app_mode_common.h"

namespace {

const int kErrorReturnValue = 1;

typedef int (*StartFun)(const app_mode::ChromeAppModeInfo*);

int LoadFrameworkAndStart(int argc, char** argv) {
  base::CommandLine command_line(argc, argv);

  @autoreleasepool {
    // Get the current main bundle, i.e., that of the app loader that's running.
    NSBundle* app_bundle = NSBundle.mainBundle;
    if (!app_bundle) {
      NSLog(@"Couldn't get loader bundle");
      return kErrorReturnValue;
    }
    const base::FilePath app_mode_bundle_path =
        base::apple::NSStringToFilePath([app_bundle bundlePath]);

    // Get the bundle ID of the browser that created this app bundle.
    NSString* cr_bundle_id = base::apple::ObjCCast<NSString>(
        [app_bundle objectForInfoDictionaryKey:app_mode::kBrowserBundleIDKey]);
    if (!cr_bundle_id) {
      NSLog(@"Couldn't get browser bundle ID");
      return kErrorReturnValue;
    }

    // ** 1: Get path to outer Chrome bundle.
    base::FilePath cr_bundle_path;
    if (command_line.HasSwitch(app_mode::kLaunchedByChromeBundlePath)) {
      // If Chrome launched this app shim, and specified its bundle path on the
      // command line, use that.
      cr_bundle_path = command_line.GetSwitchValuePath(
          app_mode::kLaunchedByChromeBundlePath);
    } else {
      // Otherwise, search for a Chrome bundle to use.
      if (!app_mode::FindChromeBundle(cr_bundle_id, &cr_bundle_path)) {
        // TODO(crbug.com/41448206): Display UI to inform the user of the
        // reason for failure.
        NSLog(@"Failed to locate browser bundle");
        return kErrorReturnValue;
      }
      if (cr_bundle_path.empty()) {
        NSLog(@"Browser bundle path unexpectedly empty");
        return kErrorReturnValue;
      }
    }

    // ** 2: Read the user data dir.
    base::FilePath user_data_dir;
    {
      // The user_data_dir for shims actually contains the app_data_path.
      // I.e. <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/
      base::FilePath app_data_dir = base::apple::NSStringToFilePath([app_bundle
          objectForInfoDictionaryKey:app_mode::kCrAppModeUserDataDirKey]);
      user_data_dir = app_data_dir.DirName().DirName().DirName();
      NSLog(@"Using user data dir %s", user_data_dir.value().c_str());
      if (user_data_dir.empty())
        return kErrorReturnValue;
    }

    // ** 3: Read the Chrome executable, Chrome framework, and Chrome framework
    // dylib paths.
    app_mode::MojoIpczConfig mojo_ipcz_config =
        app_mode::MojoIpczConfig::kUseCommandLineFeatures;
    base::FilePath executable_path;
    base::FilePath framework_path;
    base::FilePath framework_dylib_path;
    if (command_line.HasSwitch(
            app_mode::kLaunchedByChromeFrameworkBundlePath) &&
        command_line.HasSwitch(app_mode::kLaunchedByChromeFrameworkDylibPath)) {
      // If Chrome launched this app shim, then it will specify the framework
      // path and version, as well as flags to enable or disable MojoIpcz as
      // needed. Do not populate `executable_path` (it is used to launch Chrome
      // if Chrome is not running, which is inapplicable here).
      framework_path = command_line.GetSwitchValuePath(
          app_mode::kLaunchedByChromeFrameworkBundlePath);
      framework_dylib_path = command_line.GetSwitchValuePath(
          app_mode::kLaunchedByChromeFrameworkDylibPath);
    } else {
      // Otherwise, read the version from the symbolic link in the user data
      // dir. If the version file does not exist, the version string will be
      // empty and app_mode::GetChromeBundleInfo will default to the latest
      // version, with MojoIpcz disabled.
      app_mode::ChromeConnectionConfig config;
      base::FilePath encoded_config;
      base::ReadSymbolicLink(
          user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName),
          &encoded_config);
      if (!encoded_config.empty()) {
        config =
            app_mode::ChromeConnectionConfig::DecodeFromPath(encoded_config);
        mojo_ipcz_config = config.is_mojo_ipcz_enabled
                               ? app_mode::MojoIpczConfig::kEnabled
                               : app_mode::MojoIpczConfig::kDisabled;
      }
      // If the version file does exist, it may have been left by a crashed
      // Chrome process. Ensure the process is still running.
      if (!config.framework_version.empty()) {
        NSArray* existing_chrome = [NSRunningApplication
            runningApplicationsWithBundleIdentifier:cr_bundle_id];
        if ([existing_chrome count] == 0) {
          NSLog(@"Disregarding framework version from symlink");
          config.framework_version.clear();
        } else {
          NSLog(@"Framework version from symlink %s",
                config.framework_version.c_str());
        }
      }
      if (!app_mode::GetChromeBundleInfo(
              cr_bundle_path, config.framework_version.c_str(),
              &executable_path, &framework_path, &framework_dylib_path)) {
        NSLog(@"Couldn't ready Chrome bundle info");
        return kErrorReturnValue;
      }
    }

    // Check if `executable_path` was overridden by tests via the command line.
    if (command_line.HasSwitch(app_mode::kLaunchChromeForTest)) {
      executable_path =
          command_line.GetSwitchValuePath(app_mode::kLaunchChromeForTest);
    }

    // ** 4: Read information from the Info.plist.
    // Read information about the this app shortcut from the Info.plist.
    // Don't check for null-ness on optional items.
    NSDictionary* info_plist = [app_bundle infoDictionary];
    if (!info_plist) {
      NSLog(@"Couldn't get loader Info.plist");
      return kErrorReturnValue;
    }

    const std::string app_mode_id =
        base::SysNSStringToUTF8(info_plist[app_mode::kCrAppModeShortcutIDKey]);
    if (!app_mode_id.size()) {
      NSLog(@"Couldn't get app shortcut ID");
      return kErrorReturnValue;
    }

    const std::string app_mode_name = base::SysNSStringToUTF8(
        info_plist[app_mode::kCrAppModeShortcutNameKey]);
    const std::string app_mode_url =
        base::SysNSStringToUTF8(info_plist[app_mode::kCrAppModeShortcutURLKey]);

    base::FilePath plist_user_data_dir = base::apple::NSStringToFilePath(
        info_plist[app_mode::kCrAppModeUserDataDirKey]);

    base::FilePath profile_dir = base::apple::NSStringToFilePath(
        info_plist[app_mode::kCrAppModeProfileDirKey]);

    // ** 5: Open the framework.
    StartFun ChromeAppModeStart = nullptr;
    NSLog(@"Using framework path %s", framework_path.value().c_str());
    NSLog(@"Loading framework dylib %s", framework_dylib_path.value().c_str());
    void* cr_dylib = dlopen(framework_dylib_path.value().c_str(), RTLD_LAZY);
    if (cr_dylib) {
      // Find the entry point.
      ChromeAppModeStart =
          (StartFun)dlsym(cr_dylib, APP_SHIM_ENTRY_POINT_NAME_STRING);
      if (!ChromeAppModeStart)
        NSLog(@"Couldn't get entry point: %s", dlerror());
    } else {
      NSLog(@"Couldn't load framework: %s", dlerror());
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
      info.mojo_ipcz_config = mojo_ipcz_config;
      return ChromeAppModeStart(&info);
    }

    // If the shim was launched by chrome, simply quit. Chrome will detect that
    // the app shim has terminated, rebuild it (if it hadn't try to do so
    // already), and launch it again.
    if (executable_path.empty()) {
      NSLog(@"Loading Chrome failed, terminating");
      return kErrorReturnValue;
    }

    NSLog(@"Loading Chrome failed, launching Chrome with command line at %s",
          executable_path.value().c_str());
    base::CommandLine cr_command_line(executable_path);
    // The user_data_dir from the plist is actually the app data dir.
    cr_command_line.AppendSwitchPath(
        switches::kUserDataDir,
        plist_user_data_dir.DirName().DirName().DirName());
    // If the shim was launched directly (instead of by Chrome), first ask
    // Chrome to launch the app. Chrome will launch the shim again, the same
    // error might occur, after which chrome will try to regenerate the
    // shim.
    cr_command_line.AppendSwitchPath(switches::kProfileDirectory, profile_dir);
    cr_command_line.AppendSwitchASCII(switches::kAppId, app_mode_id);

    // If kLaunchChromeForTest was specified, this is a launch from a test.
    // In this case make sure to tell chrome to use a mock keychain, as
    // otherwise it might hang on startup.
    if (command_line.HasSwitch(app_mode::kLaunchChromeForTest)) {
      cr_command_line.AppendSwitch("use-mock-keychain");
    }

    // Launch the executable directly since base::mac::LaunchApplication doesn't
    // pass command line arguments if the application is already running.
    if (!base::LaunchProcess(cr_command_line, base::LaunchOptions())
             .IsValid()) {
      NSLog(@"Could not launch Chrome: %s",
            cr_command_line.GetCommandLineString().c_str());
      return kErrorReturnValue;
    }

    return 0;
  }
}

} // namespace

__attribute__((visibility("default")))
int main(int argc, char** argv) {
  // The static constructor in //base will have registered PartitionAlloc as the
  // default zone. Allow the //base instance in the main library to register it
  // as well. Otherwise we end up passing memory to free() which was allocated
  // by an unknown zone. See crbug.com/1274236 for details.
  partition_alloc::AllowDoublePartitionAllocZoneRegistration();

  base::CommandLine::Init(argc, argv);

  // Exit instead of returning to avoid the the removal of |main()| from stack
  // backtraces under tail call optimization.
  exit(LoadFrameworkAndStart(argc, argv));
}
