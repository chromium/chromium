// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/launch_context.h"

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "crypto/random.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace extensions {

namespace {

// Reads path to the native messaging host manifest from a specific subkey in
// the registry. Returns false if the path isn't found.
bool GetManifestPathWithFlagsFromSubkey(HKEY root_key,
                                        DWORD flags,
                                        const wchar_t* subkey,
                                        const std::wstring& host_name,
                                        std::wstring* result) {
  base::win::RegKey key;

  return key.Open(root_key, subkey, KEY_QUERY_VALUE | flags) == ERROR_SUCCESS &&
         key.OpenKey(host_name.c_str(), KEY_QUERY_VALUE | flags) ==
             ERROR_SUCCESS &&
         key.ReadValue(nullptr, result) == ERROR_SUCCESS;
}

// Reads path to the native messaging host manifest from the registry. Returns
// false if the path isn't found.
bool GetManifestPathWithFlags(HKEY root_key,
                              DWORD flags,
                              const std::wstring& host_name,
                              std::wstring* result) {
#if BUILDFLAG(CHROMIUM_BRANDING)
  static constexpr wchar_t kChromiumNativeMessagingRegistryKey[] =
      L"SOFTWARE\\Chromium\\NativeMessagingHosts";

  // Try to read the path using the Chromium-specific registry for Chromium.
  // If that fails, fallback to Chrome-specific registry key below.
  if (GetManifestPathWithFlagsFromSubkey(root_key, flags,
                                         kChromiumNativeMessagingRegistryKey,
                                         host_name, result)) {
    return true;
  }
#endif

  static constexpr wchar_t kChromeNativeMessagingRegistryKey[] =
      L"SOFTWARE\\Google\\Chrome\\NativeMessagingHosts";

  return GetManifestPathWithFlagsFromSubkey(
      root_key, flags, kChromeNativeMessagingRegistryKey, host_name, result);
}

bool GetManifestPath(HKEY root_key,
                     const std::wstring& host_name,
                     std::wstring* result) {
  // First check 32-bit registry and then try 64-bit.
  return GetManifestPathWithFlags(root_key, KEY_WOW64_32KEY, host_name,
                                  result) ||
         GetManifestPathWithFlags(root_key, KEY_WOW64_64KEY, host_name, result);
}

// If the Host is an executable, we will invoke it directly to avoid problems
// if CMD.exe is unavailable due to OS policy or other configuration issues
// on the client. See https://crbug.com/335558 for details.
base::Process LaunchNativeExeDirectly(const std::wstring& command,
                                      base::LaunchOptions& options,
                                      const std::wstring& in_pipe_name,
                                      const std::wstring& out_pipe_name) {
  // When calling the Host executable directly, we must first wrap our Named
  // Pipes into HANDLEs returned from CreateFileW(). We must configure the
  // handle's security attributes to allow the file handle to be inherited
  // into the launched process.
  SECURITY_ATTRIBUTES sa_attr = {};
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = nullptr;

  base::win::ScopedHandle stdout_file(
      ::CreateFileW(out_pipe_name.c_str(), FILE_WRITE_DATA | SYNCHRONIZE, 0,
                    &sa_attr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
  if (!stdout_file.IsValid()) {
    LOG(ERROR) << "Failed to open write handle for stdout.";
    return base::Process();
  }

  base::win::ScopedHandle stdin_file(
      ::CreateFileW(in_pipe_name.c_str(), FILE_READ_DATA | SYNCHRONIZE, 0,
                    &sa_attr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
  if (!stdin_file.IsValid()) {
    LOG(ERROR) << "Failed to open read handle for stdin.";
    return base::Process();
  }

  options.stdin_handle = stdin_file.Get();
  options.stdout_handle = stdout_file.Get();
  options.stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
  options.handles_to_inherit.push_back(options.stdin_handle);
  options.handles_to_inherit.push_back(options.stdout_handle);

  // Inherit Chrome's STD_ERROR_HANDLE, if set, into the Native Host. If Chrome
  // was not started with standard error redirected, this value will be null.
  if (options.stderr_handle != NULL) {
    options.handles_to_inherit.push_back(options.stderr_handle);
  }

  return base::LaunchProcess(command, options);
}

// For non-executable Hosts, we will use the legacy approach whereby we
// invoke CMD.exe and instruct it to launch the Host, passing references to
// our Named Pipes.
base::Process LaunchNativeHostViaCmd(const std::wstring& command,
                                     base::LaunchOptions& options,
                                     const std::wstring& in_pipe_name,
                                     const std::wstring& out_pipe_name) {
  DWORD comspec_length = ::GetEnvironmentVariable(L"COMSPEC", NULL, 0);
  if (comspec_length == 0) {
    LOG(ERROR) << "COMSPEC is not set";
    return base::Process();
  }
  std::wstring comspec;
  ::GetEnvironmentVariable(
      L"COMSPEC", base::WriteInto(&comspec, comspec_length), comspec_length);

  return base::LaunchProcess(
      base::StrCat({comspec, L" /d /s /c \"", command, L"\" < ", in_pipe_name,
                    L" > ", out_pipe_name}),
      options);
}

}  // namespace

// static
base::FilePath LaunchContext::FindManifest(const std::string& host_name,
                                           bool allow_user_level_hosts,
                                           std::string& error_message) {
  std::wstring host_name_wide = base::UTF8ToWide(host_name);

  // If permitted, look in HKEY_CURRENT_USER first. If the manifest isn't found
  // there, then try HKEY_LOCAL_MACHINE. https://crbug.com/1034919#c6
  std::wstring path_str;
  bool found = false;
  if (allow_user_level_hosts) {
    found = GetManifestPath(HKEY_CURRENT_USER, host_name_wide, &path_str);
  }
  if (!found) {
    found = GetManifestPath(HKEY_LOCAL_MACHINE, host_name_wide, &path_str);
  }

  if (!found) {
    error_message =
        "Native messaging host " + host_name + " is not registered.";
    return base::FilePath();
  }

  base::FilePath manifest_path(path_str);
  if (!manifest_path.IsAbsolute()) {
    error_message = "Path to native messaging host manifest must be absolute.";
    return base::FilePath();
  }

  return manifest_path;
}

// static
std::optional<LaunchContext::ProcessState> LaunchContext::LaunchNativeProcess(
    const base::CommandLine& command_line,
    bool native_hosts_executables_launch_directly) {
  // Timeout for the IO pipes.
  const DWORD kTimeoutMs = 5000;

  // Windows will use default buffer size when 0 is passed to
  // CreateNamedPipeW().
  const DWORD kBufferSize = 0;

  if (!command_line.GetProgram().IsAbsolute()) {
    LOG(ERROR) << "Native Messaging host path must be absolute.";
    return std::nullopt;
  }

  uint64_t pipe_name_token;
  crypto::RandBytes(base::byte_span_from_ref(pipe_name_token));
  const std::wstring pipe_name_token_str =
      base::ASCIIToWide(base::StringPrintf("%llx", pipe_name_token));
  const std::wstring out_pipe_name =
      L"\\\\.\\pipe\\chrome.nativeMessaging.out." + pipe_name_token_str;
  const std::wstring in_pipe_name =
      L"\\\\.\\pipe\\chrome.nativeMessaging.in." + pipe_name_token_str;

  // Create the pipes to read and write from.
  base::win::ScopedHandle stdout_pipe(::CreateNamedPipeW(
      out_pipe_name.c_str(),
      PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED |
          FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize, kTimeoutMs, NULL));
  if (!stdout_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << out_pipe_name;
    return std::nullopt;
  }

  base::win::ScopedHandle stdin_pipe(::CreateNamedPipeW(
      in_pipe_name.c_str(),
      PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
          FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize, kTimeoutMs, NULL));
  if (!stdin_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << in_pipe_name;
    return std::nullopt;
  }

  std::wstring command = command_line.GetCommandLineString();
  base::LaunchOptions options;
  options.current_directory = command_line.GetProgram().DirName();
  options.start_hidden = true;

  const bool use_direct_launch =
      native_hosts_executables_launch_directly &&
      command_line.GetProgram().MatchesFinalExtension(L".exe");

  base::Process launched_process;
  if (use_direct_launch) {
    // Compat: If the target is SUBSYSTEM_WINDOWS, then don't set |start_hidden|
    // in order to mimic legacy behavior: https://crbug.com/1442359.
    // A Windows executable will have LOWORD of 0x4550. A GUI executable will
    // have a non-Zero HIWORD while a console executable will have a 0 HIWORD.
    uintptr_t exe_type = ::SHGetFileInfoW(
        command_line.GetProgram().value().c_str(), 0, NULL, 0, SHGFI_EXETYPE);
    if ((LOWORD(exe_type) == 0x4550) && (HIWORD(exe_type) != 0)) {
      options.start_hidden = false;
    }

    launched_process =
        LaunchNativeExeDirectly(command, options, in_pipe_name, out_pipe_name);
  } else {
    launched_process =
        LaunchNativeHostViaCmd(command, options, in_pipe_name, out_pipe_name);
  }

  if (!launched_process.IsValid()) {
    LOG(ERROR) << "Error launching process "
               << command_line.GetProgram().MaybeAsASCII();
    return std::nullopt;
  }

  return ProcessState(std::move(launched_process), std::move(stdout_pipe),
                      std::move(stdin_pipe));
}

void LaunchContext::ConnectPipes(base::ScopedPlatformFile read_file,
                                 base::ScopedPlatformFile write_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(native_process_.IsValid());

  read_stream_ = std::make_unique<net::FileStream>(
      base::File(std::move(read_file), /*async=*/true),
      background_task_runner_);
  write_stream_ = std::make_unique<net::FileStream>(
      base::File(std::move(write_file), /*async=*/true),
      background_task_runner_);

  const int read_result = read_stream_->ConnectNamedPipe(
      base::BindOnce(&LaunchContext::OnReadStreamConnectResult, GetWeakPtr()));
  const int write_result = write_stream_->ConnectNamedPipe(
      base::BindOnce(&LaunchContext::OnWriteStreamConnectResult, GetWeakPtr()));

  read_pipe_connected_ = read_result == net::OK;
  write_pipe_connected_ = write_result == net::OK;

  if ((!read_pipe_connected_ && read_result != net::ERR_IO_PENDING) ||
      (!write_pipe_connected_ && write_result != net::ERR_IO_PENDING)) {
    // Failed connecting to one of the pipes to talk to the host.
    OnFailure(NativeProcessLauncher::RESULT_FAILED_TO_START);
    return;
  }

  if (read_pipe_connected_ && write_pipe_connected_) {
    // The host has already connected to both pipes.
    OnSuccess(base::kInvalidPlatformFile, std::move(read_stream_),
              std::move(write_stream_));
    return;
  }

  // Wait for calls to one or both of the StreamConnectResult methods once the
  // connect operation(s) complete. In the meantime, watch the host process to
  // make sure it doesn't tip over while waiting.
  process_watcher_.StartWatchingOnce(native_process_.Handle(), this);
}

void LaunchContext::OnReadStreamConnectResult(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (net_error != net::OK) {
    // Failed to connect.
    OnFailure(NativeProcessLauncher::RESULT_FAILED_TO_START);
    return;
  }
  // The host has connected to the read pipe.
  read_pipe_connected_ = true;
  OnPipeConnected();
}

void LaunchContext::OnWriteStreamConnectResult(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (net_error != net::OK) {
    // Failed to connect.
    OnFailure(NativeProcessLauncher::RESULT_FAILED_TO_START);
    return;
  }
  // The host has connected to the write pipe.
  write_pipe_connected_ = true;
  OnPipeConnected();
}

void LaunchContext::OnPipeConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!read_pipe_connected_ || !write_pipe_connected_) {
    return;  // At least one pipe's result is still outstanding.
  }
  process_watcher_.StopWatching();
  OnSuccess(base::kInvalidPlatformFile, std::move(read_stream_),
            std::move(write_stream_));
}

void LaunchContext::OnObjectSignaled(HANDLE object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The host process has terminated unexpectedly.
  CHECK_EQ(object, native_process_.Handle());
  int exit_code = 0;  // EXIT_SUCCESS
  if (native_process_.WaitForExitWithTimeout({}, &exit_code)) {
    LOG(ERROR) << "Native Messaging host process exited with code "
               << exit_code;
  } else {
    LOG(ERROR) << "Native Messaging host process exited unexpectedly";
  }
  OnFailure(NativeProcessLauncher::RESULT_FAILED_TO_START);
}

}  // namespace extensions
