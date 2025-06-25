// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/browser_restart_job.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_commands {

BrowserRestartJob::BrowserRestartJob() = default;

BrowserRestartJob::~BrowserRestartJob() = default;

enterprise_management::RemoteCommand_Type BrowserRestartJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_BROWSER_RESTART;
}

bool BrowserRestartJob::ParseCommandPayload(
    const std::string& command_payload) {
  // No actual payload required.
  return true;
}

void BrowserRestartJob::RunImpl(CallbackWithResult result_callback) {
  VLOG_POLICY(1, REMOTE_COMMANDS) << "Executing browser restart command.";

  // The launch time of the browser.
  //
  // TODO(nicolaso): This doesn't account for clock drift or timezone changes.
  // An alternative approach is to write unique_id() on disk (e.g. via
  // PrefService) with the command id *before* AttemptRestart(), and use that as
  // our source of truth.
  const base::TimeTicks launch_time =
      g_browser_process->browser_policy_connector()->browser_launch_time();

  // If the reboot command was issued before the browser launched, we inform the
  // server that the restart succeeded. Otherwise, the restart must still be
  // performed and we invoke it.
  if (launch_time > issued_time()) {
    LOG(WARNING) << "Ignoring restart command issued "
                 << (launch_time - issued_time())
                 << " before current browser launch time";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  policy::ResultType::kSuccess, std::nullopt));
    return;
  }

  chrome::RelaunchIgnoreUnloadHandlers();
}

}  // namespace enterprise_commands
