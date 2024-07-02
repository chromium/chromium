// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/files/platform_file.h"
#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class FileStream;
}

class Profile;

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

  // Callback that's called after the process has been launched. Handler must
  // take ownership of the process and streams. `read_file`, supplied only on
  // POSIX, is the file descriptor owned by `read_stream`.
  using LaunchedCallback =
      base::OnceCallback<void(LaunchResult result,
                              base::Process process,
                              base::PlatformFile read_file,
                              std::unique_ptr<net::FileStream> read_stream,
                              std::unique_ptr<net::FileStream> write_stream)>;

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
      const std::string& error_arg,
      Profile* profile);

  NativeProcessLauncher(const NativeProcessLauncher&) = delete;
  NativeProcessLauncher& operator=(const NativeProcessLauncher&) = delete;

  virtual ~NativeProcessLauncher() = default;

  // Finds native messaging host with the specified name and launches it
  // asynchronously. Also checks that the specified |origin| is permitted to
  // access the host. |callback| is called after the process has been started.
  // If the launcher is destroyed before the callback is called then the call is
  // canceled and the process is stopped if it has been started already (by
  // closing IO pipes).
  virtual void Launch(const GURL& origin,
                      const std::string& native_host_name,
                      LaunchedCallback callback) const = 0;

 protected:
  NativeProcessLauncher() = default;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_PROCESS_LAUNCHER_H_
