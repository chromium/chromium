// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/logging/logging.h"

#include <cstdio>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
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

void LogFileSetUp(const base::CommandLine& command_line,
                  const base::FilePath& log_path,
                  const base::FilePath& target_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The |log_path| is the new log file after log rotation. so it shouldn't be
  // deleted even if it already exists.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::DetermineLoggingDestination(command_line);
  settings.log_file_path = log_path.value().c_str();
  if (!logging::InitLogging(settings)) {
    DLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&logging::RemoveSymlinkAndLog, log_path, target_path));
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

  // Redirect logs to the session log directory, if set.  Otherwise
  // defaults to the profile dir.
  const base::FilePath log_path = logging::GetSessionLogFile(command_line);

  LOG(WARNING) << "Redirecting post-login logging to " << log_path.value();

  // Rotate the old log files when redirecting.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&logging::SetUpLogFile, log_path, /*new_log=*/true),
      base::BindOnce(&LogFileSetUp, command_line, log_path));
}

}  // namespace ash
