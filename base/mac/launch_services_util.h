// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_LAUNCH_SERVICES_UTIL_H_
#define BASE_MAC_LAUNCH_SERVICES_UTIL_H_

#import <AppKit/AppKit.h>

#include "base/base_export.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace base::mac {

struct OpenApplicationOptions {
  bool activate = true;
  bool create_new_instance = false;
};

using ApplicationOpenedCallback =
    base::OnceCallback<void(NSRunningApplication*, NSError*)>;

// Launches the specified application bundle.
//   - `app_bundle_path`: the location of the application to launch
//   - `command_line`: the arguments (argv[1..]) to pass to the application as
//      command line arguments if the app isn't already running
//   - `url_specs`: the URLs for the application to open (an empty vector is OK)
//   - `options`: options to modify the launch
//   - `callback`: the result callback
//
// When the launch is complete, `callback` is called on the main thread. If the
// launch succeeded, it will be called with a non-nil `NSRunningApplication*`
// and nil `NSError*`. If the launch failed, it will be called with a nil
// `NSRunningApplication*` and non-nil `NSError*`.
BASE_EXPORT void OpenApplication(const FilePath& app_bundle_path,
                                 const CommandLine& command_line,
                                 const std::vector<std::string>& url_specs,
                                 OpenApplicationOptions options,
                                 ApplicationOpenedCallback callback);

}  // namespace base::mac

#endif  // BASE_MAC_LAUNCH_SERVICES_UTIL_H_
