// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <windows.h>
#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "crypto/random.h"

namespace extensions {

const wchar_t kChromeNativeMessagingRegistryKey[] =
    L"SOFTWARE\\Google\\Chrome\\NativeMessagingHosts";
#if BUILDFLAG(CHROMIUM_BRANDING)
const wchar_t kChromiumNativeMessagingRegistryKey[] =
    L"SOFTWARE\\Chromium\\NativeMessagingHosts";
#endif

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
  // Try to read the path using the Chromium-specific registry for Chromium.
  // If that fails, fallback to Chrome-specific registry key below.
  if (GetManifestPathWithFlagsFromSubkey(root_key, flags,
                                         kChromiumNativeMessagingRegistryKey,
                                         host_name, result)) {
    return true;
  }
#endif

  return GetManifestPathWithFlagsFromSubkey(
      root_key, flags, kChromeNativeMessagingRegistryKey, host_name, result);
}

bool GetManifestPath(HKEY root_key,
                     const std::wstring& host_name,
                     std::wstring* result) {
  // First check 32-bit registry and then try 64-bit.
  return GetManifestPathWithFlags(
             root_key, KEY_WOW64_32KEY, host_name, result) ||
         GetManifestPathWithFlags(
             root_key, KEY_WOW64_64KEY, host_name, result);
}

}  // namespace

// static
base::FilePath NativeProcessLauncher::FindManifest(
    const std::string& host_name,
    bool allow_user_level_hosts,
    std::string* error_message) {
  std::wstring host_name_wide = base::UTF8ToWide(host_name);

  // If permitted, look in HKEY_CURRENT_USER first. If the manifest isn't found
  // there, then try HKEY_LOCAL_MACHINE. https://crbug.com/1034919#c6
  std::wstring path_str;
  bool found = false;
  if (allow_user_level_hosts)
    found = GetManifestPath(HKEY_CURRENT_USER, host_name_wide, &path_str);
  if (!found)
    found = GetManifestPath(HKEY_LOCAL_MACHINE, host_name_wide, &path_str);

  if (!found) {
    *error_message =
        "Native messaging host " + host_name + " is not registered.";
    return base::FilePath();
  }

  base::FilePath manifest_path(path_str);
  if (!manifest_path.IsAbsolute()) {
    *error_message = "Path to native messaging host manifest must be absolute.";
    return base::FilePath();
  }

  return manifest_path;
}

// static
bool NativeProcessLauncher::LaunchNativeProcess(
    const base::CommandLine& command_line,
    base::Process* process,
    base::File* read_file,
    base::File* write_file) {
  // Timeout for the IO pipes.
  const DWORD kTimeoutMs = 5000;

  // Windows will use default buffer size when 0 is passed to
  // CreateNamedPipeW().
  const DWORD kBufferSize = 0;

  if (!command_line.GetProgram().IsAbsolute()) {
    LOG(ERROR) << "Native Messaging host path must be absolute.";
    return false;
  }

  uint64_t pipe_name_token;
  crypto::RandBytes(&pipe_name_token, sizeof(pipe_name_token));
  std::wstring out_pipe_name = base::StringPrintf(
      L"\\\\.\\pipe\\chrome.nativeMessaging.out.%llx", pipe_name_token);
  std::wstring in_pipe_name = base::StringPrintf(
      L"\\\\.\\pipe\\chrome.nativeMessaging.in.%llx", pipe_name_token);

  // Create the pipes to read and write from.
  base::win::ScopedHandle stdout_pipe(
      CreateNamedPipeW(out_pipe_name.c_str(),
                       PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED |
                           FILE_FLAG_FIRST_PIPE_INSTANCE,
                       PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize,
                       kTimeoutMs, NULL));
  if (!stdout_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << out_pipe_name;
    return false;
  }

  base::win::ScopedHandle stdin_pipe(
      CreateNamedPipeW(in_pipe_name.c_str(),
                       PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                           FILE_FLAG_FIRST_PIPE_INSTANCE,
                       PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize,
                       kTimeoutMs, NULL));
  if (!stdin_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << in_pipe_name;
    return false;
  }

  DWORD comspec_length = ::GetEnvironmentVariable(L"COMSPEC", NULL, 0);
  if (comspec_length == 0) {
    LOG(ERROR) << "COMSPEC is not set";
    return false;
  }
  std::unique_ptr<wchar_t[]> comspec(new wchar_t[comspec_length]);
  ::GetEnvironmentVariable(L"COMSPEC", comspec.get(), comspec_length);

  std::wstring command_line_string = command_line.GetCommandLineString();

  std::wstring command = base::StringPrintf(
      L"%ls /d /c %ls < %ls > %ls", comspec.get(), command_line_string.c_str(),
      in_pipe_name.c_str(), out_pipe_name.c_str());

  base::LaunchOptions options;
  options.start_hidden = true;
  options.current_directory = command_line.GetProgram().DirName();
  base::Process cmd_process = base::LaunchProcess(command, options);
  if (!cmd_process.IsValid()) {
    LOG(ERROR) << "Error launching process "
               << command_line.GetProgram().MaybeAsASCII();
    return false;
  }

  bool stdout_connected = ConnectNamedPipe(stdout_pipe.Get(), NULL) ?
      TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
  bool stdin_connected = ConnectNamedPipe(stdin_pipe.Get(), NULL) ?
      TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
  if (!stdout_connected || !stdin_connected) {
    cmd_process.Terminate(0, false);
    LOG(ERROR) << "Failed to connect IO pipes when starting "
               << command_line.GetProgram().MaybeAsASCII();
    return false;
  }

  *process = std::move(cmd_process);
  *read_file = base::File(std::move(stdout_pipe), true /* async */);
  *write_file = base::File(std::move(stdin_pipe), true /* async */);
  return true;
}

}  // namespace extensions
