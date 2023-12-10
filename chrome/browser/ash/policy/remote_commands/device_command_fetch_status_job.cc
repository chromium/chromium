// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_status_job.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

DeviceCommandFetchStatusJob::DeviceCommandFetchStatusJob() {}

DeviceCommandFetchStatusJob::~DeviceCommandFetchStatusJob() {}

enterprise_management::RemoteCommand_Type DeviceCommandFetchStatusJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_FETCH_STATUS;
}

void DeviceCommandFetchStatusJob::RunImpl(CallbackWithResult result_callback) {
  SYSLOG(INFO) << "Fetching device status";
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DeviceCloudPolicyManagerAsh* manager =
      connector->GetDeviceCloudPolicyManager();
  // DeviceCloudPolicyManagerAsh, StatusUploader and SystemLogUploader can
  // be null during shutdown (and unit tests). StatusUploader and
  // SystemLogUploader objects have the same lifetime - they are created
  // together and they are destroyed together (which is why this code doesn't
  // do separate checks for them before using them).
  if (manager && manager->GetStatusUploader() &&
      manager->GetSystemLogUploader()) {
    manager->GetStatusUploader()->ScheduleNextStatusUploadImmediately();
    manager->GetSystemLogUploader()->ScheduleNextSystemLogUploadImmediately(
        unique_id());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kSuccess, std::nullopt));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                ResultType::kFailure, std::nullopt));
}

}  // namespace policy
