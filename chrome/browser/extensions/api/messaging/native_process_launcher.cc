// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {

namespace {

// Default implementation on NativeProcessLauncher interface.
class NativeProcessLauncherImpl : public NativeProcessLauncher {
 public:
  NativeProcessLauncherImpl(bool allow_user_level_hosts,
                            intptr_t native_window);
  ~NativeProcessLauncherImpl() override;

  void Launch(const GURL& origin,
              const std::string& native_host_name,
              const LaunchedCallback& callback) const override;

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(bool allow_user_level_hosts, intptr_t native_window);
    void Launch(const GURL& origin,
                const std::string& native_host_name,
                const LaunchedCallback& callback);
    void Detach();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void DoLaunchOnThreadPool(const GURL& origin,
                              const std::string& native_host_name,
                              const LaunchedCallback& callback);
    void PostErrorResult(const LaunchedCallback& callback, LaunchResult error);
    void PostResult(const LaunchedCallback& callback,
                    base::Process process,
                    base::File read_file,
                    base::File write_file);
    void CallCallbackOnIOThread(const LaunchedCallback& callback,
                                LaunchResult result,
                                base::Process process,
                                base::File read_file,
                                base::File write_file);

    bool detached_;

    bool allow_user_level_hosts_;

#if defined(OS_WIN)
    // Handle of the native window corresponding to the extension.
    intptr_t window_handle_;
#endif // OS_WIN

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(NativeProcessLauncherImpl);
};

NativeProcessLauncherImpl::Core::Core(bool allow_user_level_hosts,
                                      intptr_t window_handle)
    : detached_(false),
      allow_user_level_hosts_(allow_user_level_hosts)
#if defined(OS_WIN)
      , window_handle_(window_handle)
#endif // OS_WIN
{}

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
    const LaunchedCallback& callback) {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
                           base::BindOnce(&Core::DoLaunchOnThreadPool, this,
                                          origin, native_host_name, callback));
}

void NativeProcessLauncherImpl::Core::DoLaunchOnThreadPool(
    const GURL& origin,
    const std::string& native_host_name,
    const LaunchedCallback& callback) {
  if (!NativeMessagingHostManifest::IsValidName(native_host_name)) {
    PostErrorResult(callback, RESULT_INVALID_NAME);
    return;
  }

  std::string error_message;
  base::FilePath manifest_path =
      FindManifest(native_host_name, allow_user_level_hosts_, &error_message);

  if (manifest_path.empty()) {
    LOG(WARNING) << "Can't find manifest for native messaging host "
                 << native_host_name;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  std::unique_ptr<NativeMessagingHostManifest> manifest =
      NativeMessagingHostManifest::Load(manifest_path, &error_message);

  if (!manifest) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name << ": " << error_message;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  if (manifest->name() != native_host_name) {
    LOG(WARNING) << "Failed to load manifest for native messaging host "
                 << native_host_name
                 << ": Invalid name specified in the manifest.";
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  if (!manifest->allowed_origins().MatchesSecurityOrigin(origin)) {
    // Not an allowed origin.
    PostErrorResult(callback, RESULT_FORBIDDEN);
    return;
  }

  base::FilePath host_path = manifest->path();
  if (!host_path.IsAbsolute()) {
    // On Windows host path is allowed to be relative to the location of the
    // manifest file. On all other platforms the path must be absolute.
#if defined(OS_WIN)
    host_path = manifest_path.DirName().Append(host_path);
#else  // defined(OS_WIN)
    LOG(WARNING) << "Native messaging host path must be absolute for "
                 << native_host_name;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
#endif  // !defined(OS_WIN)
  }

  // In case when the manifest file is there, but the host binary doesn't exist
  // report the NOT_FOUND error.
  if (!base::PathExists(host_path)) {
    LOG(WARNING)
        << "Found manifest, but not the binary for native messaging host "
        << native_host_name << ". Host path specified in the manifest: "
        << host_path.AsUTF8Unsafe();
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  base::CommandLine command_line(host_path);
  // Note: The origin must be the first argument, so do not use AppendSwitch*
  // hereafter because CommandLine inserts these switches before the other
  // arguments.
  command_line.AppendArg(origin.spec());

  // Pass handle of the native view window to the native messaging host. This
  // way the host will be able to create properly focused UI windows.
#if defined(OS_WIN)
  command_line.AppendArg(
      base::StringPrintf("--parent-window=%" PRIdPTR, window_handle_));
#endif  // !defined(OS_WIN)

  base::Process process;
  base::File read_file;
  base::File write_file;
  if (NativeProcessLauncher::LaunchNativeProcess(
          command_line, &process, &read_file, &write_file)) {
    PostResult(callback, std::move(process), std::move(read_file),
               std::move(write_file));
  } else {
    PostErrorResult(callback, RESULT_FAILED_TO_START);
  }
}

void NativeProcessLauncherImpl::Core::CallCallbackOnIOThread(
    const LaunchedCallback& callback,
    LaunchResult result,
    base::Process process,
    base::File read_file,
    base::File write_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (detached_)
    return;

  callback.Run(result, std::move(process), std::move(read_file),
               std::move(write_file));
}

void NativeProcessLauncherImpl::Core::PostErrorResult(
    const LaunchedCallback& callback,
    LaunchResult error) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                     this, callback, error, base::Process(), base::File(),
                     base::File()));
}

void NativeProcessLauncherImpl::Core::PostResult(
    const LaunchedCallback& callback,
    base::Process process,
    base::File read_file,
    base::File write_file) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                     this, callback, RESULT_SUCCESS, std::move(process),
                     std::move(read_file), std::move(write_file)));
}

NativeProcessLauncherImpl::NativeProcessLauncherImpl(
    bool allow_user_level_hosts,
    intptr_t window_handle)
    : core_(new Core(allow_user_level_hosts, window_handle)) {
}

NativeProcessLauncherImpl::~NativeProcessLauncherImpl() {
  core_->Detach();
}

void NativeProcessLauncherImpl::Launch(const GURL& origin,
                                       const std::string& native_host_name,
                                       const LaunchedCallback& callback) const {
  core_->Launch(origin, native_host_name, callback);
}

}  // namespace

// static
std::unique_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault(
    bool allow_user_level_hosts,
    gfx::NativeView native_view) {
  intptr_t window_handle = 0;
#if defined(OS_WIN)
  window_handle = reinterpret_cast<intptr_t>(
      views::HWNDForNativeView(native_view));
#endif
  return std::unique_ptr<NativeProcessLauncher>(
      new NativeProcessLauncherImpl(allow_user_level_hosts, window_handle));
}

}  // namespace extensions
