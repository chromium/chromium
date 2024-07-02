// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/launch_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/file_stream.h"

namespace extensions {

namespace {

base::FilePath FindManifestInDir(int dir_key, const std::string& host_name) {
  base::FilePath base_path;
  if (base::PathService::Get(dir_key, &base_path)) {
    base::FilePath path = base_path.Append(host_name + ".json");
    if (base::PathExists(path)) {
      return path;
    }
  }
  return base::FilePath();
}

}  // namespace

// static
base::FilePath LaunchContext::FindManifest(const std::string& host_name,
                                           bool allow_user_level_hosts,
                                           std::string& error_message) {
  base::FilePath result;
  if (allow_user_level_hosts) {
    result = FindManifestInDir(chrome::DIR_USER_NATIVE_MESSAGING, host_name);
  }
  if (result.empty()) {
    result = FindManifestInDir(chrome::DIR_NATIVE_MESSAGING, host_name);
  }

  if (result.empty()) {
    error_message = "Can't find native messaging host " + host_name;
  }

  return result;
}

// static
std::optional<LaunchContext::ProcessState> LaunchContext::LaunchNativeProcess(
    const base::CommandLine& command_line,
    // This is only relevant on Windows
    bool native_hosts_executables_launch_directly) {
  base::LaunchOptions options;

  int read_pipe_fds[2] = {0};
  if (HANDLE_EINTR(pipe(read_pipe_fds)) != 0) {
    LOG(ERROR) << "Bad read pipe";
    return std::nullopt;
  }
  base::ScopedFD read_pipe_read_fd(read_pipe_fds[0]);
  base::ScopedFD read_pipe_write_fd(read_pipe_fds[1]);
  options.fds_to_remap.push_back(
      std::make_pair(read_pipe_write_fd.get(), STDOUT_FILENO));

  int write_pipe_fds[2] = {0};
  if (HANDLE_EINTR(pipe(write_pipe_fds)) != 0) {
    LOG(ERROR) << "Bad write pipe";
    return std::nullopt;
  }
  base::ScopedFD write_pipe_read_fd(write_pipe_fds[0]);
  base::ScopedFD write_pipe_write_fd(write_pipe_fds[1]);
  options.fds_to_remap.push_back(
      std::make_pair(write_pipe_read_fd.get(), STDIN_FILENO));

  options.current_directory = command_line.GetProgram().DirName();

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Don't use no_new_privs mode, e.g. in case the host needs to use sudo.
  options.allow_new_privs = true;
#endif

#if BUILDFLAG(IS_MAC)
  // This is executing a third-party binary, so do not associate any system
  // private data requests with Chrome.
  options.disclaim_responsibility = true;
#endif

  base::Process local_process = base::LaunchProcess(command_line, options);
  if (!local_process.IsValid()) {
    LOG(ERROR) << "Error launching process";
    return std::nullopt;
  }

  // We will not be reading from the write pipe, nor writing from the read pipe.
  write_pipe_read_fd.reset();
  read_pipe_write_fd.reset();

  return ProcessState(std::move(local_process), std::move(read_pipe_read_fd),
                      std::move(write_pipe_write_fd));
}

void LaunchContext::ConnectPipes(base::ScopedPlatformFile read_file,
                                 base::ScopedPlatformFile write_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(native_process_.IsValid());

  base::PlatformFile read_file_unowned = read_file.get();
  OnSuccess(read_file_unowned,
            std::make_unique<net::FileStream>(base::File(std::move(read_file)),
                                              background_task_runner_),
            std::make_unique<net::FileStream>(base::File(std::move(write_file)),
                                              background_task_runner_));
}

}  // namespace extensions
