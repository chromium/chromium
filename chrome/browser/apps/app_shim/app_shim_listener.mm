// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_listener.h"

#import <Foundation/Foundation.h>
#include <bsm/libbsm.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/variations/net/variations_command_line.h"
#include "content/public/browser/browser_task_traits.h"

AppShimListener::AppShimListener() = default;

void AppShimListener::Init() {
  has_initialized_ = true;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Initialize the instance of AppShimTerminationManager, to ensure that it
  // registers for its notifications.
  apps::AppShimTerminationManager::Get();

  // If running the shim triggers Chrome startup, the user must wait for the
  // socket to be set up before the shim will be usable. This also requires
  // IO, so use MayBlock() with USER_VISIBLE.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&AppShimListener::InitOnBackgroundThread, this));
}

AppShimListener::~AppShimListener() {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(mach_acceptor_));

  // The AppShimListener is only initialized if the Chrome process
  // successfully took the singleton lock. If it was not initialized, do not
  // delete existing app shim socket files as they belong to another process.
  if (!has_initialized_)
    return;

  AppShimHostBootstrap::SetClient(nullptr);

  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    base::FilePath version_path =
        user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);

    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::GetDeleteFileCallback(version_path));
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

  // Create a symlink containing the current version string and a bit indicating
  // whether or not the MojoIpcz feature is enabled. This allows the shim to
  // load the same framework version as the currently running Chrome process,
  // and it ensures that both processes are using the same IPC implementation.
  base::FilePath version_path =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  const auto config =
      app_mode::ChromeConnectionConfig::GenerateForCurrentProcess();
  base::DeleteFile(version_path);
  base::CreateSymbolicLink(config.EncodeAsPath(), version_path);

  if (!variations::VariationsCommandLine::GetForCurrentProcess().WriteToFile(
          user_data_dir.Append(app_mode::kFeatureStateFileName))) {
    LOG(ERROR) << "Failed to write feature state to " << user_data_dir;
  }
}

void AppShimListener::OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                                        audit_token_t audit_token) {
  // TODO(crbug.com/40674145): Remove NSLog logging, and move to an
  // internal debugging URL.
  NSLog(@"AppShim: Connection received from pid %d",
        audit_token_to_pid(audit_token));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AppShimHostBootstrap::CreateForChannelAndPeerAuditToken,
                     std::move(endpoint), audit_token));
}

void AppShimListener::OnServerChannelCreateError() {
  // TODO(crbug.com/41035623): Set a timeout and attempt to reconstruct
  // the channel. Until cases where the error could occur are better known,
  // just reset the acceptor to allow failure to be communicated via the test
  // API.
  mach_acceptor_.reset();
}
