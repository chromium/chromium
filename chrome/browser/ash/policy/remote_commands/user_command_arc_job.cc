// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_command_arc_job.h"

#include <utility>

#include "ash/components/arc/mojom/policy.mojom.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

constexpr base::TimeDelta kDefaultCommandTimeout = base::Minutes(2);

}  // namespace

UserCommandArcJob::UserCommandArcJob(Profile* profile) : profile_(profile) {}

UserCommandArcJob::~UserCommandArcJob() = default;

enterprise_management::RemoteCommand_Type UserCommandArcJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_USER_ARC_COMMAND;
}

base::TimeDelta UserCommandArcJob::GetCommandTimeout() const {
  return kDefaultCommandTimeout;
}

bool UserCommandArcJob::ParseCommandPayload(
    const std::string& command_payload) {
  command_payload_ = command_payload;
  return true;
}

void UserCommandArcJob::RunImpl(CallbackWithResult result_callback) {
  // Payload may contain crypto key, thus only log payload in debugging mode.
  SYSLOG(INFO) << "Running Arc command...";
  DLOG(INFO) << "payload = " << command_payload_;

  auto* const arc_policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);

  if (!arc_policy_bridge) {
    // ARC is not enabled for this profile, fail the remote command.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::nullopt));
    return;
  }

  auto on_command_finished_callback = base::BindOnce(
      [](CallbackWithResult result_callback,
         arc::mojom::CommandResultType result) {
        bool command_failed =
            result == arc::mojom::CommandResultType::FAILURE ||
            result == arc::mojom::CommandResultType::IGNORED;
        std::move(result_callback)
            .Run(command_failed ? ResultType::kFailure : ResultType::kSuccess,
                 std::nullopt);
      },
      std::move(result_callback));

  // Documentation for RemoteCommandJob::RunImpl requires that the
  // implementation executes the command asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&arc::ArcPolicyBridge::OnCommandReceived,
                     arc_policy_bridge->GetWeakPtr(), command_payload_,
                     std::move(on_command_finished_callback)));
}

}  // namespace policy
