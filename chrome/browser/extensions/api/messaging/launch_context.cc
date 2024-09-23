// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/launch_context.h"

#include <inttypes.h>

#include <utility>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "net/base/file_stream.h"

namespace extensions {

namespace {

void TerminateNativeProcess(base::Process native_process) {
// Kill the host process if necessary to make sure we don't leave zombies.
// TODO(crbug.com/41367359): On OSX EnsureProcessTerminated() may
// block, so we have to post a task on the blocking pool.
#if BUILDFLAG(IS_MAC)
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&base::EnsureProcessTerminated,
                                            std::move(native_process)));
#else
  base::EnsureProcessTerminated(std::move(native_process));
#endif
}

}  // namespace

// static
std::unique_ptr<LaunchContext> LaunchContext::Start(
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
    NativeProcessLauncher::LaunchedCallback callback) {
  // Must run on the IO thread.
  CHECK(base::CurrentIOThread::IsSet());

  auto instance = base::WrapUnique(
      new LaunchContext(background_task_runner, std::move(callback)));
  background_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LaunchInBackground, allow_user_level_hosts,
                     require_native_initiated_connections,
                     native_hosts_executables_launch_directly, window_handle,
                     std::move(profile_directory), std::move(connect_id),
                     std::move(error_arg), std::move(origin),
                     std::move(native_host_name)),
      base::BindOnce(&OnProcessLaunched, instance->GetWeakPtr()));
  return instance;
}

LaunchContext::~LaunchContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

LaunchContext::LaunchContext(
    scoped_refptr<base::TaskRunner> background_task_runner,
    NativeProcessLauncher::LaunchedCallback callback)
    : background_task_runner_(std::move(background_task_runner)),
      callback_(std::move(callback)) {}

LaunchContext::ProcessState::ProcessState() = default;
LaunchContext::ProcessState::ProcessState(base::Process process,
                                          base::ScopedPlatformFile read_file,
                                          base::ScopedPlatformFile write_file)
    : process(std::move(process)),
      read_file(std::move(read_file)),
      write_file(std::move(write_file)) {}
LaunchContext::ProcessState::ProcessState(ProcessState&& other) noexcept =
    default;
LaunchContext::ProcessState& LaunchContext::ProcessState::operator=(
    ProcessState&& other) noexcept = default;
LaunchContext::ProcessState::~ProcessState() = default;

LaunchContext::BackgroundLaunchResult::BackgroundLaunchResult(
    NativeProcessLauncher::LaunchResult result)
    : result(result), process_state(std::nullopt) {}
LaunchContext::BackgroundLaunchResult::BackgroundLaunchResult(
    ProcessState process_state)
    : result(NativeProcessLauncher::RESULT_SUCCESS),
      process_state(std::move(process_state)) {}
LaunchContext::BackgroundLaunchResult::BackgroundLaunchResult(
    BackgroundLaunchResult&& other) noexcept = default;
LaunchContext::BackgroundLaunchResult&
LaunchContext::BackgroundLaunchResult::operator=(
    BackgroundLaunchResult&& other) noexcept = default;
LaunchContext::BackgroundLaunchResult::~BackgroundLaunchResult() = default;

// static
LaunchContext::BackgroundLaunchResult LaunchContext::LaunchInBackground(
    bool allow_user_level_hosts,
    bool require_native_initiated_connections,
    bool native_hosts_executables_launch_directly,
    intptr_t window_handle,
    const base::FilePath& profile_directory,
    const std::string& connect_id,
    const std::string& error_arg,
    const GURL& origin,
    const std::string& native_host_name) {
  if (!NativeMessagingHostManifest::IsValidName(native_host_name)) {
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_INVALID_NAME);
  }

  std::string error_message;
  base::FilePath manifest_path =
      FindManifest(native_host_name, allow_user_level_hosts, error_message);

  if (manifest_path.empty()) {
    LOG(WARNING) << "Can't find manifest for native messaging host "
                 << native_host_name;
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_NOT_FOUND);
  }

  std::unique_ptr<NativeMessagingHostManifest> manifest =
      NativeMessagingHostManifest::Load(manifest_path, &error_message);

  if (!manifest) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name << ": " << error_message;
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_NOT_FOUND);
  }

  if (manifest->name() != native_host_name) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name
                 << ": Invalid name specified in the manifest.";
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_NOT_FOUND);
  }

  if (!manifest->allowed_origins().MatchesSecurityOrigin(origin)) {
    // Not an allowed origin.
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_FORBIDDEN);
  }

  if (require_native_initiated_connections &&
      !manifest->supports_native_initiated_connections()) {
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_FORBIDDEN);
  }

  base::FilePath host_path = manifest->path();
  if (!host_path.IsAbsolute()) {
    // On Windows host path is allowed to be relative to the location of the
    // manifest file. On all other platforms the path must be absolute.
#if BUILDFLAG(IS_WIN)
    host_path = manifest_path.DirName().Append(host_path);
#else   // BUILDFLAG(IS_WIN)
    LOG(WARNING) << "Native messaging host path must be absolute for "
                 << native_host_name;
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_NOT_FOUND);
#endif  // BUILDFLAG(IS_WIN)
  }

  // In case when the manifest file is there, but the host binary doesn't exist
  // report the NOT_FOUND error.
  if (!base::PathExists(host_path)) {
    LOG(WARNING)
        << "Found manifest, but not the binary for native messaging host "
        << native_host_name << ". Host path specified in the manifest: "
        << host_path.AsUTF8Unsafe();
    return BackgroundLaunchResult(NativeProcessLauncher::RESULT_NOT_FOUND);
  }

  base::CommandLine command_line(host_path);
  // Note: The origin must be the first argument, so do not use AppendSwitch*
  // hereafter because CommandLine inserts these switches before the other
  // arguments.
  command_line.AppendArg(origin.spec());

  // Pass handle of the native view window to the native messaging host. This
  // way the host will be able to create properly focused UI windows.
#if BUILDFLAG(IS_WIN)
  command_line.AppendArg(
      base::StringPrintf("--parent-window=%" PRIdPTR, window_handle));
#endif  // !BUILDFLAG(IS_WIN)

  bool send_connect_id = false;
  if (!error_arg.empty()) {
    send_connect_id = true;
    command_line.AppendArg(error_arg);
  } else if (manifest->supports_native_initiated_connections() &&
             !profile_directory.empty()) {
    send_connect_id = true;
    base::FilePath exe_path;
    base::PathService::Get(base::FILE_EXE, &exe_path);

    base::CommandLine reconnect_command_line(exe_path);
    reconnect_command_line.AppendSwitch(::switches::kNoStartupWindow);
    reconnect_command_line.AppendSwitchASCII(
        ::switches::kNativeMessagingConnectHost, native_host_name);
    reconnect_command_line.AppendSwitchASCII(
        ::switches::kNativeMessagingConnectExtension, origin.host());
    reconnect_command_line.AppendSwitchASCII(::switches::kEnableFeatures,
                                             features::kOnConnectNative.name);
    reconnect_command_line.AppendSwitchPath(::switches::kProfileDirectory,
                                            profile_directory.BaseName());
    reconnect_command_line.AppendSwitchPath(::switches::kUserDataDir,
                                            profile_directory.DirName());
#if BUILDFLAG(IS_WIN)
    reconnect_command_line.AppendArgNative(
        app_launch_prefetch::GetPrefetchSwitch(
            app_launch_prefetch::SubprocessType::kBrowserBackground));
#endif
    base::Value::List args;
    for (const auto& arg : reconnect_command_line.argv()) {
#if BUILDFLAG(IS_WIN)
      args.Append(base::WideToUTF8(arg));
#else
      args.Append(arg);
#endif
    }
    std::string encoded_reconnect_command;
    bool success =
        base::JSONWriter::Write(std::move(args), &encoded_reconnect_command);
    DCHECK(success);
    command_line.AppendArg(
        base::StrCat({"--reconnect-command=",
                      base::Base64Encode(encoded_reconnect_command)}));
  }

  if (send_connect_id && !connect_id.empty()) {
    command_line.AppendArg(base::StrCat(
        {"--", switches::kNativeMessagingConnectId, "=", connect_id}));
  }

  if (auto state = LaunchNativeProcess(
          command_line, native_hosts_executables_launch_directly)) {
    return BackgroundLaunchResult(*std::move(state));
  }
  return BackgroundLaunchResult(NativeProcessLauncher::RESULT_FAILED_TO_START);
}

// static
void LaunchContext::OnProcessLaunched(base::WeakPtr<LaunchContext> weak_this,
                                      BackgroundLaunchResult result) {
  if (!weak_this) {
    // The launch was cancelled. Terminate the host process and close the pipes.
    if (result.process_state.has_value()) {
      auto& process_state = result.process_state.value();
      TerminateNativeProcess(std::move(process_state.process));
      // Close the pipes on a background thread since doing so may block.
      // Ownership of the pipes is passed to a callback that is run in the
      // thread pool. They will be closed automatically when they go out of
      // scope when the callback is run.
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(
              [](base::ScopedPlatformFile read_file,
                 base::ScopedPlatformFile write_file) {
                // Execution of this lambda will lead to destruction of
                // `read_file` and `write_file`.
              },
              std::move(process_state.read_file),
              std::move(process_state.write_file)));
    }
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(weak_this->sequence_checker_);
  if (result.result == NativeProcessLauncher::RESULT_SUCCESS) {
    weak_this->native_process_ = std::move(result.process_state->process);
    weak_this->ConnectPipes(std::move(result.process_state->read_file),
                            std::move(result.process_state->write_file));
  } else {
    weak_this->OnFailure(result.result);
  }
}

void LaunchContext::OnSuccess(base::PlatformFile read_file,
                              std::unique_ptr<net::FileStream> read_stream,
                              std::unique_ptr<net::FileStream> write_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(NativeProcessLauncher::RESULT_SUCCESS,
                           std::move(native_process_), read_file,
                           std::move(read_stream), std::move(write_stream));
}

void LaunchContext::OnFailure(
    NativeProcessLauncher::LaunchResult launch_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (native_process_.IsValid()) {
    TerminateNativeProcess(std::move(native_process_));
  }
  std::move(callback_).Run(launch_result, base::Process(),
                           base::kInvalidPlatformFile, nullptr, nullptr);
}

}  // namespace extensions
