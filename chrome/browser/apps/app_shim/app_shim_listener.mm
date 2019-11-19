// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_listener.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"

AppShimListener::AppShimListener() {}

void AppShimListener::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!extension_app_shim_handler_);
  extension_app_shim_handler_.reset(new apps::ExtensionAppShimHandler());
  AppShimHostBootstrap::SetClient(extension_app_shim_handler_.get());
  // Initialize the instance of AppShimTerminationManager, to ensure that it
  // registers for its notifications.
  apps::AppShimTerminationManager::Get();

  // If running the shim triggers Chrome startup, the user must wait for the
  // socket to be set up before the shim will be usable. This also requires
  // IO, so use MayBlock() with USER_VISIBLE.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&AppShimListener::InitOnBackgroundThread, this));
}

AppShimListener::~AppShimListener() {
  base::CreateSingleThreadTaskRunner({content::BrowserThread::IO})
      ->DeleteSoon(FROM_HERE, std::move(mach_acceptor_));

  // The AppShimListener is only initialized if the Chrome process
  // successfully took the singleton lock. If it was not initialized, do not
  // delete existing app shim socket files as they belong to another process.
  if (!extension_app_shim_handler_)
    return;

  AppShimHostBootstrap::SetClient(nullptr);

  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    base::FilePath version_path =
        user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);

    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), version_path,
                       false));
  }
}

void AppShimListener::InitOnBackgroundThread() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return;

  std::string name_fragment =
      std::string(app_mode::kAppShimBootstrapNameFragment) + "." +
      base::MD5String(user_data_dir.value());
  mach_acceptor_ =
      std::make_unique<apps::MachBootstrapAcceptor>(name_fragment, this);
  mach_acceptor_->Start();

  // Create a symlink containing the current version string. This allows the
  // shim to load the same framework version as the currently running Chrome
  // process.
  base::FilePath version_path =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  base::DeleteFile(version_path, false);
  base::CreateSymbolicLink(base::FilePath(version_info::GetVersionNumber()),
                           version_path);
}

void AppShimListener::OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                                        base::ProcessId peer_pid) {
  base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&AppShimHostBootstrap::CreateForChannelAndPeerID,
                         std::move(endpoint), peer_pid));
}

void AppShimListener::OnServerChannelCreateError() {
  // TODO(https://crbug.com/272577): Set a timeout and attempt to reconstruct
  // the channel. Until cases where the error could occur are better known,
  // just reset the acceptor to allow failure to be communicated via the test
  // API.
  mach_acceptor_.reset();
}
