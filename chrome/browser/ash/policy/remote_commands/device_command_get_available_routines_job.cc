// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_available_routines_job.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the routines field in the result payload.
constexpr char kRoutinesFieldName[] = "routines";

std::string CreatePayload(
    const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines) {
  base::Value::List routine_list;
  for (const auto& routine : available_routines) {
    routine_list.Append(static_cast<int>(routine));
  }
  auto root_dict =
      base::Value::Dict().Set(kRoutinesFieldName, std::move(routine_list));

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

}  // namespace

DeviceCommandGetAvailableRoutinesJob::DeviceCommandGetAvailableRoutinesJob() =
    default;

DeviceCommandGetAvailableRoutinesJob::~DeviceCommandGetAvailableRoutinesJob() =
    default;

em::RemoteCommand_Type DeviceCommandGetAvailableRoutinesJob::GetType() const {
  return em::RemoteCommand_Type_DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES;
}

void DeviceCommandGetAvailableRoutinesJob::RunImpl(
    CallbackWithResult result_callback) {
  SYSLOG(INFO) << "Executing GetAvailableRoutines command.";

  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetDiagnosticsService()
      ->GetAvailableRoutines(base::BindOnce(
          &DeviceCommandGetAvailableRoutinesJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void DeviceCommandGetAvailableRoutinesJob::OnCrosHealthdResponseReceived(
    CallbackWithResult result_callback,
    const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines) {
  if (available_routines.empty()) {
    SYSLOG(ERROR) << "No routines received from cros_healthd.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::nullopt));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), ResultType::kSuccess,
                     CreatePayload(available_routines)));
}

}  // namespace policy
