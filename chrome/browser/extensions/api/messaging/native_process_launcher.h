// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace base {
class CommandLine;
class FilePath;
}

namespace extensions {

class NativeProcessLauncher {
 public:
  enum LaunchResult {
    RESULT_SUCCESS,
    RESULT_INVALID_NAME,
    RESULT_NOT_FOUND,
    RESULT_FORBIDDEN,
    RESULT_FAILED_TO_START,
  };

  // Callback that's called after the process has been launched. |result| is set
  // to false in case of a failure. Handler must take ownership of the IO
  // handles.
  typedef base::Callback<void(LaunchResult result,
                              base::Process process,
                              base::File read_file,
                              base::File write_file)> LaunchedCallback;

  // Creates default launcher for the current OS. |native_view| refers to the
  // window that contains calling page. Can be nullptr, e.g. for background
  // pages. If |profile_directory| is non-empty and the host supports
  // native-initiated connections, additional reconnect args will be passed to
  // the host. If |require_native_initiated_connections| is true, the connection
  // will be allowed only if the native messaging host sets
  // "supports_native_initiated_connections" to true in its manifest.
  // If |error_arg| is non-empty, the reconnect args are omitted, and instead
  // the error value is passed as a command line argument to the host.
  static std::unique_ptr<NativeProcessLauncher> CreateDefault(
      bool allow_user_level_hosts,
      gfx::NativeView native_view,
      const base::FilePath& profile_directory,
      bool require_native_initiated_connections,
      const std::string& connect_id,
      const std::string& error_arg);

  NativeProcessLauncher() {}
  virtual ~NativeProcessLauncher() {}

  // Finds native messaging host with the specified name and launches it
  // asynchronously. Also checks that the specified |origin| is permitted to
  // access the host. |callback| is called after the process has been started.
  // If the launcher is destroyed before the callback is called then the call is
  // canceled and the process is stopped if it has been started already (by
  // closing IO pipes).
  virtual void Launch(const GURL& origin,
                      const std::string& native_host_name,
                      const LaunchedCallback& callback) const = 0;

 protected:
  // The following two methods are platform specific and are implemented in
  // platform-specific .cc files.

  // Finds manifest file for the native messaging host |native_host_name|.
  // |user_level| is set to true if the manifest is installed on user level.
  // Returns an empty path if the host with the specified name cannot be found.
  static base::FilePath FindManifest(const std::string& native_host_name,
                                     bool allow_user_level_hosts,
                                     std::string* error_message);

  // Launches native messaging process.
  static bool LaunchNativeProcess(const base::CommandLine& command_line,
                                  base::Process* process,
                                  base::File* read_file,
                                  base::File* write_file);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeProcessLauncher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
