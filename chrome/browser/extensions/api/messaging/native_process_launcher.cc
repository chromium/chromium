// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <inttypes.h>

#include <utility>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {

namespace {

// Default implementation on NativeProcessLauncher interface.
class NativeProcessLauncherImpl : public NativeProcessLauncher {
 public:
  NativeProcessLauncherImpl(bool allow_user_level_hosts,
                            intptr_t native_window,
                            const base::FilePath& profile_directory,
                            bool require_native_initiated_connections,
                            const std::string& connect_id,
                            const std::string& error_arg);

  NativeProcessLauncherImpl(const NativeProcessLauncherImpl&) = delete;
  NativeProcessLauncherImpl& operator=(const NativeProcessLauncherImpl&) =
      delete;

  ~NativeProcessLauncherImpl() override;

  void Launch(const GURL& origin,
              const std::string& native_host_name,
              LaunchedCallback callback) const override;

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(bool allow_user_level_hosts,
         intptr_t native_window,
         const base::FilePath& profile_directory,
         bool require_native_initiated_connections,
         const std::string& connect_id,
         const std::string& error_arg);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    void Launch(const GURL& origin,
                const std::string& native_host_name,
                LaunchedCallback callback);
    void Detach();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void DoLaunchOnThreadPool(const GURL& origin,
                              const std::string& native_host_name,
                              LaunchedCallback callback);
    void PostErrorResult(LaunchedCallback callback, LaunchResult error);
    void PostResult(LaunchedCallback callback,
                    base::Process process,
                    base::File read_file,
                    base::File write_file);
    void CallCallbackOnIOThread(LaunchedCallback callback,
                                LaunchResult result,
                                base::Process process,
                                base::File read_file,
                                base::File write_file);

    bool detached_;

    const bool allow_user_level_hosts_;

    const base::FilePath profile_directory_;

    const bool require_native_initiated_connections_;

    const std::string connect_id_;
    const std::string error_arg_;

#if BUILDFLAG(IS_WIN)
    // Handle of the native window corresponding to the extension.
    intptr_t window_handle_;
#endif  // BUILDFLAG(IS_WIN)
  };

  scoped_refptr<Core> core_;
};

NativeProcessLauncherImpl::Core::Core(bool allow_user_level_hosts,
                                      intptr_t window_handle,
                                      const base::FilePath& profile_directory,
                                      bool require_native_initiated_connections,
                                      const std::string& connect_id,
                                      const std::string& error_arg)
    : detached_(false),
      allow_user_level_hosts_(allow_user_level_hosts),
      profile_directory_(profile_directory),
      require_native_initiated_connections_(
          require_native_initiated_connections),
      connect_id_(connect_id),
      error_arg_(error_arg)
#if BUILDFLAG(IS_WIN)
      ,
      window_handle_(window_handle)
#endif  // BUILDFLAG(IS_WIN)
{
}

NativeProcessLauncherImpl::Core::~Core() {
  DCHECK(detached_);
}

void NativeProcessLauncherImpl::Core::Detach() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  detached_ = true;
}

void NativeProcessLauncherImpl::Core::Launch(
    const GURL& origin,
    const std::string& native_host_name,
    LaunchedCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Core::DoLaunchOnThreadPool, this, origin,
                     native_host_name, std::move(callback)));
}

void NativeProcessLauncherImpl::Core::DoLaunchOnThreadPool(
    const GURL& origin,
    const std::string& native_host_name,
    LaunchedCallback callback) {
  if (!NativeMessagingHostManifest::IsValidName(native_host_name)) {
    PostErrorResult(std::move(callback), RESULT_INVALID_NAME);
    return;
  }

  std::string error_message;
  base::FilePath manifest_path =
      FindManifest(native_host_name, allow_user_level_hosts_, &error_message);

  if (manifest_path.empty()) {
    LOG(WARNING) << "Can't find manifest for native messaging host "
                 << native_host_name;
    PostErrorResult(std::move(callback), RESULT_NOT_FOUND);
    return;
  }

  std::unique_ptr<NativeMessagingHostManifest> manifest =
      NativeMessagingHostManifest::Load(manifest_path, &error_message);

  if (!manifest) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name << ": " << error_message;
    PostErrorResult(std::move(callback), RESULT_NOT_FOUND);
    return;
  }

  if (manifest->name() != native_host_name) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name
                 << ": Invalid name specified in the manifest.";
    PostErrorResult(std::move(callback), RESULT_NOT_FOUND);
    return;
  }

  if (!manifest->allowed_origins().MatchesSecurityOrigin(origin)) {
    // Not an allowed origin.
    PostErrorResult(std::move(callback), RESULT_FORBIDDEN);
    return;
  }

  if (require_native_initiated_connections_ &&
      !manifest->supports_native_initiated_connections()) {
    PostErrorResult(std::move(callback), RESULT_FORBIDDEN);
    return;
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
    PostErrorResult(std::move(callback), RESULT_NOT_FOUND);
    return;
#endif  // BUILDFLAG(IS_WIN)
  }

  // In case when the manifest file is there, but the host binary doesn't exist
  // report the NOT_FOUND error.
  if (!base::PathExists(host_path)) {
    LOG(WARNING)
        << "Found manifest, but not the binary for native messaging host "
        << native_host_name << ". Host path specified in the manifest: "
        << host_path.AsUTF8Unsafe();
    PostErrorResult(std::move(callback), RESULT_NOT_FOUND);
    return;
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
      base::StringPrintf("--parent-window=%" PRIdPTR, window_handle_));
#endif  // !BUILDFLAG(IS_WIN)

  bool send_connect_id = false;
  if (!error_arg_.empty()) {
    send_connect_id = true;
    command_line.AppendArg(error_arg_);
  } else if (manifest->supports_native_initiated_connections() &&
             !profile_directory_.empty()) {
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
                                            profile_directory_.BaseName());
    reconnect_command_line.AppendSwitchPath(::switches::kUserDataDir,
                                            profile_directory_.DirName());
#if BUILDFLAG(IS_WIN)
    reconnect_command_line.AppendArg(
        ::switches::kPrefetchArgumentBrowserBackground);
#endif
    base::Value args(base::Value::Type::LIST);
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
    base::Base64Encode(encoded_reconnect_command, &encoded_reconnect_command);
    command_line.AppendArg(
        base::StrCat({"--reconnect-command=", encoded_reconnect_command}));
  }

  if (send_connect_id && !connect_id_.empty()) {
    command_line.AppendArg(base::StrCat(
        {"--", switches::kNativeMessagingConnectId, "=", connect_id_}));
  }

  base::Process process;
  base::File read_file;
  base::File write_file;
  if (NativeProcessLauncher::LaunchNativeProcess(
          command_line, &process, &read_file, &write_file)) {
    PostResult(std::move(callback), std::move(process), std::move(read_file),
               std::move(write_file));
  } else {
    PostErrorResult(std::move(callback), RESULT_FAILED_TO_START);
  }
}

void NativeProcessLauncherImpl::Core::CallCallbackOnIOThread(
    LaunchedCallback callback,
    LaunchResult result,
    base::Process process,
    base::File read_file,
    base::File write_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (detached_)
    return;

  std::move(callback).Run(result, std::move(process), std::move(read_file),
                          std::move(write_file));
}

void NativeProcessLauncherImpl::Core::PostErrorResult(LaunchedCallback callback,
                                                      LaunchResult error) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                     this, std::move(callback), error, base::Process(),
                     base::File(), base::File()));
}

void NativeProcessLauncherImpl::Core::PostResult(LaunchedCallback callback,
                                                 base::Process process,
                                                 base::File read_file,
                                                 base::File write_file) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                     this, std::move(callback), RESULT_SUCCESS,
                     std::move(process), std::move(read_file),
                     std::move(write_file)));
}

NativeProcessLauncherImpl::NativeProcessLauncherImpl(
    bool allow_user_level_hosts,
    intptr_t window_handle,
    const base::FilePath& profile_directory,
    bool require_native_initiated_connections,
    const std::string& connect_id,
    const std::string& error_arg)
    : core_(base::MakeRefCounted<Core>(allow_user_level_hosts,
                                       window_handle,
                                       profile_directory,
                                       require_native_initiated_connections,
                                       connect_id,
                                       error_arg)) {}

NativeProcessLauncherImpl::~NativeProcessLauncherImpl() {
  core_->Detach();
}

void NativeProcessLauncherImpl::Launch(const GURL& origin,
                                       const std::string& native_host_name,
                                       LaunchedCallback callback) const {
  core_->Launch(origin, native_host_name, std::move(callback));
}

}  // namespace

// static
std::unique_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault(
    bool allow_user_level_hosts,
    gfx::NativeView native_view,
    const base::FilePath& profile_directory,
    bool require_native_initiated_connections,
    const std::string& connect_id,
    const std::string& error_arg) {
  intptr_t window_handle = 0;
#if BUILDFLAG(IS_WIN)
  window_handle = reinterpret_cast<intptr_t>(
      views::HWNDForNativeView(native_view));
#endif
  return std::make_unique<NativeProcessLauncherImpl>(
      allow_user_level_hosts, window_handle, profile_directory,
      require_native_initiated_connections, connect_id, error_arg);
}

}  // namespace extensions
