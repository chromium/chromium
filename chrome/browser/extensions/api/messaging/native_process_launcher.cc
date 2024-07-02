// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/launch_context.h"
#include "net/base/file_stream.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension_features.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {

namespace {

// Default implementation on NativeProcessLauncher interface.
class NativeProcessLauncherImpl : public NativeProcessLauncher {
 public:
  NativeProcessLauncherImpl(bool allow_user_level_hosts,
                            bool require_native_initiated_connections,
                            bool native_hosts_executables_launch_directly,
                            intptr_t window_handle,
                            const base::FilePath& profile_directory,
                            const std::string& connect_id,
                            const std::string& error_arg);

  NativeProcessLauncherImpl(const NativeProcessLauncherImpl&) = delete;
  NativeProcessLauncherImpl& operator=(const NativeProcessLauncherImpl&) =
      delete;

  void Launch(const GURL& origin,
              const std::string& native_host_name,
              LaunchedCallback callback) const override;

 private:
  void OnComplete(LaunchedCallback callback,
                  LaunchResult result,
                  base::Process process,
                  base::PlatformFile read_file,
                  std::unique_ptr<net::FileStream> read_stream,
                  std::unique_ptr<net::FileStream> write_stream) const;

  const bool allow_user_level_hosts_;
  const bool require_native_initiated_connections_;
  const bool native_hosts_executables_launch_directly_;
  // Handle of the native window corresponding to the extension.
  const intptr_t window_handle_;
  const base::FilePath profile_directory_;
  const std::string connect_id_;
  const std::string error_arg_;

  // An in-progress launch.
  mutable std::unique_ptr<LaunchContext> context_;
};

NativeProcessLauncherImpl::NativeProcessLauncherImpl(
    bool allow_user_level_hosts,
    bool require_native_initiated_connections,
    bool native_hosts_executables_launch_directly,
    intptr_t window_handle,
    const base::FilePath& profile_directory,
    const std::string& connect_id,
    const std::string& error_arg)
    : allow_user_level_hosts_(allow_user_level_hosts),
      require_native_initiated_connections_(
          require_native_initiated_connections),
      native_hosts_executables_launch_directly_(
          native_hosts_executables_launch_directly),
      window_handle_(window_handle),
      profile_directory_(profile_directory),
      connect_id_(connect_id),
      error_arg_(error_arg) {}

void NativeProcessLauncherImpl::Launch(const GURL& origin,
                                       const std::string& native_host_name,
                                       LaunchedCallback callback) const {
  CHECK(!context_);  // Parallel launches are not supported.
  context_ = LaunchContext::Start(
      allow_user_level_hosts_, require_native_initiated_connections_,
      native_hosts_executables_launch_directly_, window_handle_,
      profile_directory_, connect_id_, error_arg_, origin, native_host_name,
      base::ThreadPool::CreateTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()}),
      // `Unretained` is safe here, as `LaunchContext` guarantees that it will
      // never run this callback after the context is destroyed.
      base::BindOnce(&NativeProcessLauncherImpl::OnComplete,
                     base::Unretained(this), std::move(callback)));
}

void NativeProcessLauncherImpl::OnComplete(
    LaunchedCallback callback,
    LaunchResult result,
    base::Process process,
    base::PlatformFile read_file,
    std::unique_ptr<net::FileStream> read_stream,
    std::unique_ptr<net::FileStream> write_stream) const {
  context_.reset();

  std::move(callback).Run(result, std::move(process), read_file,
                          std::move(read_stream), std::move(write_stream));
}

}  // namespace

// static
std::unique_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault(
    bool allow_user_level_hosts,
    gfx::NativeView native_view,
    const base::FilePath& profile_directory,
    bool require_native_initiated_connections,
    const std::string& connect_id,
    const std::string& error_arg,
    Profile* profile) {
  intptr_t window_handle = 0;
  bool native_hosts_executables_launch_directly = false;
#if BUILDFLAG(IS_WIN)
  window_handle = reinterpret_cast<intptr_t>(
      views::HWNDForNativeView(native_view));

  if (profile && profile->GetPrefs()->IsManagedPreference(
                     prefs::kNativeHostsExecutablesLaunchDirectly)) {
    native_hosts_executables_launch_directly = profile->GetPrefs()->GetBoolean(
        prefs::kNativeHostsExecutablesLaunchDirectly);
  } else {
    native_hosts_executables_launch_directly = base::FeatureList::IsEnabled(
        extensions_features::kLaunchWindowsNativeHostsDirectly);
  }
#endif  // BUILDFLAG(IS_WIN)

  return std::make_unique<NativeProcessLauncherImpl>(
      allow_user_level_hosts, require_native_initiated_connections,
      native_hosts_executables_launch_directly, window_handle,
      profile_directory, connect_id, error_arg);
}

}  // namespace extensions
