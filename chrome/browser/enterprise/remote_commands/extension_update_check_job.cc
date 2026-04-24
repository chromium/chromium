// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/extension_update_check_job.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "extensions/browser/extension_system.h"

namespace enterprise_commands {

ExtensionUpdateCheckJob::ExtensionUpdateCheckJob(
    ProfileManager* profile_manager)
    : job_profile_picker_(profile_manager) {}

ExtensionUpdateCheckJob::ExtensionUpdateCheckJob(Profile* profile)
    : job_profile_picker_(profile) {}

ExtensionUpdateCheckJob::~ExtensionUpdateCheckJob() = default;

enterprise_management::RemoteCommand_Type ExtensionUpdateCheckJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_EXTENSION_UPDATE_CHECK;
}

bool ExtensionUpdateCheckJob::ParseCommandPayload(
    const std::string& command_payload) {
  VLOG_POLICY(2, REMOTE_COMMANDS)
      << "Extension update check command payload: " << command_payload;
  std::optional<base::DictValue> root = base::JSONReader::ReadDict(
      command_payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!root) {
    return false;
  }

  if (!job_profile_picker_.ParseCommandPayload(*root)) {
    return false;
  }

  // No command-specific payload to parse, just the target profile.
  return true;
}

void ExtensionUpdateCheckJob::RunImpl(CallbackWithResult result_callback) {
  Profile* profile = job_profile_picker_.GetProfile();
  if (!profile) {
    // If the payload's profile path doesn't correspond to an existing profile,
    // there's nothing to do. The most likely scenario is that the profile is
    // not currently loaded.
    VLOG_POLICY(2, REMOTE_COMMANDS) << "Profile not found or not activated.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  policy::ResultType::kFailure, std::nullopt));
    return;
  }

  extensions::ExtensionUpdater* updater =
      extensions::ExtensionUpdater::Get(profile);
  CHECK(updater);

  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  params.fetch_priority = extensions::DownloadFetchPriority::kForeground;
  params.callback = base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(std::move(result_callback), policy::ResultType::kSuccess,
                     std::nullopt));

  updater->CheckNow(std::move(params));
}

}  // namespace enterprise_commands
