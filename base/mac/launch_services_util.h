// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_LAUNCH_SERVICES_UTIL_H_
#define BASE_MAC_LAUNCH_SERVICES_UTIL_H_

#import <AppKit/AppKit.h>

#include "base/base_export.h"
#include "base/command_line.h"
#include "base/files/file_path.h"

namespace base::mac {

// Launches the application bundle at |bundle_path|, passing argv[1..] from
// |command_line| as command line arguments if the app isn't already running.
// |launch_options| are passed directly to
// -[NSWorkspace launchApplicationAtURL:options:configuration:error:].
// Returns a non-nil NSRunningApplication if the app was successfully launched.
BASE_EXPORT NSRunningApplication* OpenApplicationWithPath(
    const FilePath& bundle_path,
    const CommandLine& command_line,
    NSWorkspaceLaunchOptions launch_options);

// Launches the application bundle at |bundle_path|, passing argv[1..] from
// |command_line| as command line arguments if the app isn't already running,
// and passing |urls| to the application as URLs to open.
// |launch_options| are passed directly to
// -[NSWorkspace openURLs:withApplicationAtURL:options:configuration:error:].
// Returns a non-nil NSRunningApplication if the app was successfully launched.
BASE_EXPORT NSRunningApplication* OpenApplicationWithPathAndURLs(
    const FilePath& bundle_path,
    const CommandLine& command_line,
    const std::vector<std::string>& url_specs,
    NSWorkspaceLaunchOptions launch_options);

}  // namespace base::mac

#endif  // BASE_MAC_LAUNCH_SERVICES_UTIL_H_
