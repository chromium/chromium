// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"

namespace {

base::FilePath GetDirectoryInTmpTemplate(const base::FilePath& user_data_dir) {
  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  // Check that it's shorter than the IPC socket length (104) minus the
  // intermediate folder ("/chrome-XXXXXX/") and kAppShimSocketShortName.
  DCHECK_GT(83u, temp_dir.value().length());
  return temp_dir.Append("chrome-XXXXXX");
}

void DeleteSocketFiles(const base::FilePath& directory_in_tmp,
                       const base::FilePath& symlink_path,
                       const base::FilePath& version_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  // Delete in reverse order of creation.
  if (!version_path.empty())
    base::DeleteFile(version_path, false);
  if (!symlink_path.empty())
    base::DeleteFile(symlink_path, false);
  if (!directory_in_tmp.empty())
    base::DeleteFile(directory_in_tmp, true);
}

}  // namespace

AppShimHostManager::AppShimHostManager() {}

void AppShimHostManager::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!extension_app_shim_handler_);
  extension_app_shim_handler_.reset(new apps::ExtensionAppShimHandler());
  apps::AppShimHandler::SetDefaultHandler(extension_app_shim_handler_.get());

  // If running the shim triggers Chrome startup, the user must wait for the
  // socket to be set up before the shim will be usable. This also requires
  // IO, so use MayBlock() with USER_VISIBLE.
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&AppShimHostManager::InitOnBackgroundThread, this));
}

AppShimHostManager::~AppShimHostManager() {
  acceptor_.reset();

  // The AppShimHostManager is only initialized if the Chrome process
  // successfully took the singleton lock. If it was not initialized, do not
  // delete existing app shim socket files as they belong to another process.
  if (!extension_app_shim_handler_)
    return;

  apps::AppShimHandler::SetDefaultHandler(NULL);
  base::FilePath user_data_dir;
  base::FilePath symlink_path;
  base::FilePath version_path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    symlink_path = user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);
    version_path =
        user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  }
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                           base::BindOnce(&DeleteSocketFiles, directory_in_tmp_,
                                          symlink_path, version_path));
}

void AppShimHostManager::InitOnBackgroundThread() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return;

  // The socket path must be shorter than 104 chars (IPC::kMaxSocketNameLength).
  // To accommodate this, we use a short path in /tmp/ that is generated from a
  // hash of the user data dir.
  std::string directory_string =
      GetDirectoryInTmpTemplate(user_data_dir).value();

  // mkdtemp() replaces trailing X's randomly and creates the directory.
  if (!mkdtemp(&directory_string[0])) {
    PLOG(ERROR) << directory_string;
    return;
  }

  directory_in_tmp_ = base::FilePath(directory_string);
  // Check that the directory was created with the correct permissions.
  int dir_mode = 0;
  if (!base::GetPosixFilePermissions(directory_in_tmp_, &dir_mode) ||
      dir_mode != base::FILE_PERMISSION_USER_MASK) {
    NOTREACHED();
    return;
  }

  // UnixDomainSocketAcceptor creates the socket immediately.
  base::FilePath socket_path =
      directory_in_tmp_.Append(app_mode::kAppShimSocketShortName);
  acceptor_.reset(new apps::UnixDomainSocketAcceptor(socket_path, this));

  // Create a symlink to the socket in the user data dir. This lets the shim
  // process started from Finder find the actual socket path by following the
  // symlink with ::readlink().
  base::FilePath symlink_path =
      user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);
  base::DeleteFile(symlink_path, false);
  base::CreateSymbolicLink(socket_path, symlink_path);

  // Create a symlink containing the current version string. This allows the
  // shim to load the same framework version as the currently running Chrome
  // process.
  base::FilePath version_path =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  base::DeleteFile(version_path, false);
  base::CreateSymbolicLink(base::FilePath(version_info::GetVersionNumber()),
                           version_path);

  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->PostTask(FROM_HERE,
                 base::Bind(&AppShimHostManager::ListenOnIOThread, this));
}

void AppShimHostManager::ListenOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!acceptor_->Listen()) {
    base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
        ->PostTask(FROM_HERE,
                   base::Bind(&AppShimHostManager::OnListenError, this));
  }
}

void AppShimHostManager::OnClientConnected(
    mojo::PlatformChannelEndpoint endpoint) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&AppShimHostBootstrap::CreateForChannel,
                                std::move(endpoint)));
}

void AppShimHostManager::OnListenError() {
  // TODO(tapted): Set a timeout and attempt to reconstruct the channel. Until
  // cases where the error could occur are better known, just reset the acceptor
  // to allow failure to be communicated via the test API.
  acceptor_.reset();
}
