// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>

#include "base/allocator/early_zone_registration_apple.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros_local.h"
#include "chrome/app_shim/app_shim_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"

extern "C" {
// |ChromeAppModeStart()| is the point of entry into the framework from the
// app mode loader. There are cases where the Chromium framework may have
// changed in a way that is incompatible with an older shim (e.g. change to
// libc++ library linking). The function name is versioned to provide a way
// to force shim upgrades if they are launched before an updated version of
// Chromium can upgrade them; the old shim will not be able to dyload the
// new ChromeAppModeStart, so it will fall back to the upgrade path. See
// https://crbug.com/561205.
__attribute__((visibility("default"))) int APP_SHIM_ENTRY_POINT_NAME(
    const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

int APP_SHIM_ENTRY_POINT_NAME(const app_mode::ChromeAppModeInfo* info) {
  // The static constructor in //base will have registered PartitionAlloc as
  // the default zone. Allow the //base instance in the main library to
  // register it as well. Otherwise we end up passing memory to free() which
  // was allocated by an unknown zone. See crbug.com/1274236 for details.
  partition_alloc::AllowDoublePartitionAllocZoneRegistration();

  // Set bundle paths. This loads the bundles.
  base::apple::SetOverrideOuterBundlePath(
      base::FilePath(info->chrome_outer_bundle_path));
  base::apple::SetOverrideFrameworkBundlePath(
      base::FilePath(info->chrome_framework_path));

  // Initialize the command line and append a process type switch.
  base::CommandLine::Init(info->argc, info->argv);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, switches::kAppShim);

  // Local histogram to let tests verify that histograms are emitted properly.
  LOCAL_HISTOGRAM_BOOLEAN("AppShim.Launched", true);

  AppShimMainDelegate delegate(info);
  content::ContentMainParams params(&delegate);
  return content::ContentMain(std::move(params));
}
