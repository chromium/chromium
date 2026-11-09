// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/logging/logging.h"

#include <cstdio>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/logging/log_file_util.h"
#include "chromeos/ash/components/logging/session_log.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/zygote_host/zygote_host_linux.h"

namespace ash {

namespace {

// This is true when logging redirect was tried for the first user in the
// session.
bool g_chrome_logging_redirect_tried = false;

// This should be set to true for tests that rely on log redirection.
bool g_force_log_redirection = false;

template <typename ProcessHost>
void ReinitializeLoggingForProcessHost(ProcessHost* process_host,
                                       const logging::LoggingSettings& settings,
                                       base::PlatformFile raw_log_file_fd) {
  static_assert(std::is_same<content::ChildProcessHost, ProcessHost>() ||
                std::is_same<content::RenderProcessHost, ProcessHost>());

  base::ScopedFD log_file_descriptor(HANDLE_EINTR(dup(raw_log_file_fd)));
  if (log_file_descriptor.get() < 0) {
    DLOG(WARNING) << "Unable to duplicate log file handle";
    return;
  }

  process_host->ReinitializeLogging(settings.logging_dest,
                                    std::move(log_file_descriptor));
}

void LogFileSetUp(logging::LoggingDestination logging_dest,
                  const base::FilePath& log_path,
                  const base::FilePath& target_path) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  // The |log_path| is the new log file after log rotation. so it shouldn't be
  // deleted even if it already exists.
  logging::LoggingSettings settings;
  settings.logging_dest = logging_dest;
  settings.log_file_path = log_path.value().c_str();
  if (!logging::InitLogging(settings)) {
    DLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&RemoveSymlinkAndLog, log_path, target_path));
    return;
  }

  base::ScopedFILE log_file(logging::DuplicateLogFILE());
  base::PlatformFile log_file_fd = fileno(log_file.get());
  if (log_file_fd < 0) {
    DLOG(WARNING) << "Unable to duplicate log file handle";
    return;
  }

  // Redirect Zygote and future children's logs.
  content::ZygoteHost::GetInstance()->ReinitializeLogging(settings.logging_dest,
                                                          log_file_fd);

  // Redirect child processes' logs.
  for (content::BrowserChildProcessHostIterator it; !it.Done(); ++it)
    ReinitializeLoggingForProcessHost(it.GetHost(), settings, log_file_fd);

  for (auto it(content::RenderProcessHost::AllHostsIterator()); !it.IsAtEnd();
       it.Advance()) {
    ReinitializeLoggingForProcessHost(it.GetCurrentValue(), settings,
                                      log_file_fd);
  }
}

}  // namespace

void ForceLogRedirectionForTesting() {
  g_force_log_redirection = true;
}

void RedirectChromeLogging(const base::CommandLine& command_line) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  // Only redirect when on an actual device. To do otherwise conflicts with
  // --vmodule that developers may want to use.
  if (!base::SysInfo::IsRunningOnChromeOS() && !g_force_log_redirection)
    return;

  if (g_chrome_logging_redirect_tried) {
    LOG(WARNING) << "NOT redirecting logging for multi-profiles case.";
    return;
  }

  g_chrome_logging_redirect_tried = true;

  if (command_line.HasSwitch(ash::switches::kDisableLoggingRedirect)) {
    return;
  }

  // Keep the current logging destination, which is initialized on the process
  // startup.
  const logging::LoggingDestination logging_dest =
      logging::GetLoggingDestination();
  if (!(logging_dest & logging::LOG_TO_FILE)) {
    LOG(WARNING) << "NOT redirecting logging, because logging to a file is "
                 << "not enabled.";
    return;
  }

  // The current log file is the one initialized on the process startup.
  const base::FilePath current_log_path(logging::GetLogFileFullPath());
  CHECK(!current_log_path.empty());

  // Redirect logs to the session log directory, if set.  Otherwise
  // defaults to the profile dir.
  const base::FilePath log_path =
      GetSessionLogDir().Append(current_log_path.BaseName());

  LOG(WARNING) << "Redirecting post-login logging to " << log_path.value();

  // Rotate the old log files when redirecting.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SetUpLogFile, log_path, /*new_log=*/true),
      base::BindOnce(&LogFileSetUp, logging_dest, log_path));
}

}  // namespace ash
