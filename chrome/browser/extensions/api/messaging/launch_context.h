// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_LAUNCH_CONTEXT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_LAUNCH_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/object_watcher.h"
#endif

namespace base {
class CommandLine;
class TaskRunner;
}  // namespace base

namespace net {
class FileStream;
}  // namespace net

namespace extensions {

// The state for a single native messaging host process launch. Instances live
// and die on the IO thread. Asynchronous process launch is initiated via
// `Start`. A consumer may cancel an in-progress launch by deleting the instance
// on the IO thread.
class LaunchContext
#if BUILDFLAG(IS_WIN)
    : public base::win::ObjectWatcher::Delegate
#endif
{
 public:
  // `callback` is guaranteed not to be run after the returned instance is
  // destroyed.
  static std::unique_ptr<LaunchContext> Start(
      bool allow_user_level_hosts,
      bool require_native_initiated_connections,
      bool native_hosts_executables_launch_directly,
      intptr_t window_handle,
      base::FilePath profile_directory,
      std::string connect_id,
      std::string error_arg,
      GURL origin,
      std::string native_host_name,
      scoped_refptr<base::TaskRunner> background_task_runner,
      NativeProcessLauncher::LaunchedCallback callback);
#if BUILDFLAG(IS_WIN)
  ~LaunchContext() override;
#else
  ~LaunchContext();
#endif

 private:
  LaunchContext(scoped_refptr<base::TaskRunner> background_task_runner,
                NativeProcessLauncher::LaunchedCallback callback);

  base::WeakPtr<LaunchContext> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Returns the path to the manifest file for the native messaging host
  // `host_name`. If `allow_user_level_hosts` is false, user-level manifests are
  // ignored; otherwise, they are preferred over an all-users manifest. Returns
  // an empty path if the host with the specified name cannot be found.
  static base::FilePath FindManifest(const std::string& host_name,
                                     bool allow_user_level_hosts,
                                     std::string& error_message);

  struct ProcessState {
    ProcessState();
    ProcessState(base::Process process,
                 base::ScopedPlatformFile read_file,
                 base::ScopedPlatformFile write_file);
    ProcessState(ProcessState&& other) noexcept;
    ProcessState& operator=(ProcessState&& other) noexcept;
    ~ProcessState();

    // The child process.
    base::Process process;

    // The child's stdout.
    base::ScopedPlatformFile read_file;

    // The child's stdin.
    base::ScopedPlatformFile write_file;
  };

  // Launches the native messaging process, providing it with one end each of a
  // pair of pipes for its stdout and stdin. On Windows: if
  // `native_hosts_executables_launch_directly` is true and the host is an .exe,
  // the host is launched directly; otherwise, it is launched via cmd.exe. On
  // success, returns the launched process and the out/in pipes.
  static std::optional<ProcessState> LaunchNativeProcess(
      const base::CommandLine& command_line,
      bool native_hosts_executables_launch_directly);

  // The result of a background process launch.
  struct BackgroundLaunchResult {
    explicit BackgroundLaunchResult(NativeProcessLauncher::LaunchResult result);
    explicit BackgroundLaunchResult(ProcessState process_state);
    BackgroundLaunchResult(BackgroundLaunchResult&& other) noexcept;
    BackgroundLaunchResult& operator=(BackgroundLaunchResult&& other) noexcept;
    ~BackgroundLaunchResult();

    // The result code of the launch.
    NativeProcessLauncher::LaunchResult result;

    // The handles for the child process, present only when `result` is
    // `RESULT_SUCCESS`.
    std::optional<ProcessState> process_state;
  };

  // Reads and validates the host's manifest, forms its command line, and
  // launches it. Returns an error code or the process's handles.
  static BackgroundLaunchResult LaunchInBackground(
      bool allow_user_level_hosts,
      bool require_native_initiated_connections,
      bool native_hosts_executables_launch_directly,
      intptr_t window_handle,
      const base::FilePath& profile_directory,
      const std::string& connect_id,
      const std::string& error_arg,
      const GURL& origin,
      const std::string& native_host_name);

  // Continues processing on the IO thread following `LaunchInBackground`. If
  // the launch was cancelled (via deletion of the context), the host process
  // is terminated. Otherwise, either the caller's callback is run with the
  // failure code, or the pipes are connected.
  static void OnProcessLaunched(base::WeakPtr<LaunchContext> weak_this,
                                BackgroundLaunchResult result);

  // Connects to the host process.
  void ConnectPipes(base::ScopedPlatformFile read_file,
                    base::ScopedPlatformFile write_file);

#if BUILDFLAG(IS_WIN)
  // These methods are only needed on Windows, where an extra step is needed to
  // connect to the named pipes used for stdin/stdout of the native messaging
  // host process. The connections are established asynchronously via the IO
  // completion port monitored by the IO thread.

  // Handles the result of connecting to the host's stdout pipe.
  void OnReadStreamConnectResult(int net_error);

  // Handles the result of connecting to the host's stdin pipe.
  void OnWriteStreamConnectResult(int net_error);

  // Continues processing once a pipe has connected.
  void OnPipeConnected();

  // base::win::ObjectWatcher::Delegate:
  // Handles unexpected termination of the host process.
  void OnObjectSignaled(HANDLE object) override;
#endif  // BUILDFLAG(IS_WIN)

  // Reports success via the caller's callback, which may destroy `this`.
  void OnSuccess(base::PlatformFile read_file,
                 std::unique_ptr<net::FileStream> read_stream,
                 std::unique_ptr<net::FileStream> write_stream);

  // Reports failure via the caller's callback, which may destroy `this`.
  void OnFailure(NativeProcessLauncher::LaunchResult launch_result);

  scoped_refptr<base::TaskRunner> background_task_runner_;
  NativeProcessLauncher::LaunchedCallback callback_;
  base::Process native_process_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<net::FileStream> read_stream_;
  std::unique_ptr<net::FileStream> write_stream_;
  base::win::ObjectWatcher process_watcher_;
  bool read_pipe_connected_ = false;
  bool write_pipe_connected_ = false;
#endif  // BUILDFLAG(IS_WIN)
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LaunchContext> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_LAUNCH_CONTEXT_H_
