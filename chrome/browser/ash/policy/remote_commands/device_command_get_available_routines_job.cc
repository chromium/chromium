// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_available_routines_job.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the routines field in the result payload.
constexpr char kRoutinesFieldName[] = "routines";

}  // namespace

class DeviceCommandGetAvailableRoutinesJob::Payload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit Payload(
      const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
          available_routines);
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;
  ~Payload() override = default;

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
      available_routines_;
};

DeviceCommandGetAvailableRoutinesJob::Payload::Payload(
    const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines)
    : available_routines_(available_routines) {}

std::unique_ptr<std::string>
DeviceCommandGetAvailableRoutinesJob::Payload::Serialize() {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  base::Value routine_list(base::Value::Type::LIST);
  for (const auto& routine : available_routines_)
    routine_list.Append(static_cast<int>(routine));
  root_dict.SetPath(kRoutinesFieldName, std::move(routine_list));
  base::JSONWriter::Write(root_dict, &payload);
  return std::make_unique<std::string>(std::move(payload));
}

DeviceCommandGetAvailableRoutinesJob::DeviceCommandGetAvailableRoutinesJob() =
    default;

DeviceCommandGetAvailableRoutinesJob::~DeviceCommandGetAvailableRoutinesJob() =
    default;

em::RemoteCommand_Type DeviceCommandGetAvailableRoutinesJob::GetType() const {
  return em::RemoteCommand_Type_DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES;
}

void DeviceCommandGetAvailableRoutinesJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  SYSLOG(INFO) << "Executing GetAvailableRoutines command.";

  chromeos::cros_healthd::ServiceConnection::GetInstance()
      ->GetAvailableRoutines(base::BindOnce(
          &DeviceCommandGetAvailableRoutinesJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
          std::move(failed_callback)));
}

void DeviceCommandGetAvailableRoutinesJob::OnCrosHealthdResponseReceived(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines) {
  if (available_routines.empty()) {
    SYSLOG(ERROR) << "No routines received from cros_healthd.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback), nullptr));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(succeeded_callback),
                                std::make_unique<Payload>(available_routines)));
}

}  // namespace policy
