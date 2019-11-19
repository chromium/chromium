// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "chrome/installer/util/firewall_manager_win.h"

namespace media_router {

namespace {

bool DoCanFirewallUseLocalPorts() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(WARNING) << "Couldn't get path of current executable.";
    return false;
  }
  auto firewall_manager = installer::FirewallManager::Create(exe_path);
  if (!firewall_manager) {
    LOG(WARNING) << "Couldn't get FirewallManager instance.";
    return false;
  }
  return firewall_manager->CanUseLocalPorts();
}

}  // namespace

void CanFirewallUseLocalPorts(base::OnceCallback<void(bool)> callback) {
  auto task_runner = base::CreateCOMSTATaskRunner(
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE,
                                   base::BindOnce(&DoCanFirewallUseLocalPorts),
                                   std::move(callback));
}

}  // namespace media_router
