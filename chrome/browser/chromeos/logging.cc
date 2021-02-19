// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/logging.h"

#include <cstdio>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace logging {

namespace {

// This is true when logging redirect was tried for the first user in the
// session.
bool g_chrome_logging_redirect_tried = false;

// This should be set to true for tests that rely on log redirection.
bool g_force_log_redirection = false;

void SymlinkSetUp(const base::CommandLine& command_line,
                  const base::FilePath& log_path,
                  const base::FilePath& target_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // ChromeOS always logs through the symlink, so it shouldn't be
  // deleted if it already exists.
  logging::LoggingSettings settings;
  settings.logging_dest = DetermineLoggingDestination(command_line);
  settings.log_file_path = log_path.value().c_str();
  if (!logging::InitLogging(settings)) {
    DLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&RemoveSymlinkAndLog, log_path, target_path));
    return;
  }

  // Redirect the Network Service's logs as well if it's running out of process.
  if (content::IsOutOfProcessNetworkService()) {
    auto logging_settings = network::mojom::LoggingSettings::New();
    logging_settings->logging_dest = settings.logging_dest;
    base::ScopedFD log_file_descriptor(fileno(logging::DuplicateLogFILE()));
    if (log_file_descriptor.get() < 0) {
      DLOG(WARNING) << "Unable to duplicate log file handle";
      return;
    }
    logging_settings->log_file_descriptor =
        mojo::PlatformHandle(std::move(log_file_descriptor));
    content::GetNetworkService()->ReinitializeLogging(
        std::move(logging_settings));
  }
}

}  // namespace

void ForceLogRedirectionForTesting() {
  g_force_log_redirection = true;
}

void RedirectChromeLogging(const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only redirect when on an actual device. To do otherwise conflicts with
  // --vmodule that developers may want to use.
  if (!base::SysInfo::IsRunningOnChromeOS() && !g_force_log_redirection)
    return;

  if (g_chrome_logging_redirect_tried) {
    LOG(WARNING) << "NOT redirecting logging for multi-profiles case.";
    return;
  }

  g_chrome_logging_redirect_tried = true;

  if (command_line.HasSwitch(switches::kDisableLoggingRedirect))
    return;

  LOG(WARNING)
      << "Redirecting post-login logging to /home/chronos/user/log/chrome/";

  // Redirect logs to the session log directory, if set.  Otherwise
  // defaults to the profile dir.
  const base::FilePath log_path = GetSessionLogFile(command_line);

  // Always force a new symlink when redirecting.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SetUpSymlinkIfNeeded, log_path, true),
      base::BindOnce(&SymlinkSetUp, command_line, log_path));
}

}  // namespace logging
