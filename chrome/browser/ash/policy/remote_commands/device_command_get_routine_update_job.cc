// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the output field in the result payload.
constexpr char kOutputFieldName[] = "output";
// String constant identifying the progress percent field in the result payload.
constexpr char kProgressPercentFieldName[] = "progressPercent";
// String constant identifying the noninteractive update field in the result
// payload.
constexpr char kNonInteractiveUpdateFieldName[] = "nonInteractiveUpdate";
// String constant identifying the status field in the result payload.
constexpr char kStatusFieldName[] = "status";
// String constant identifying the status message field in the result payload.
constexpr char kStatusMessageFieldName[] = "statusMessage";
// String constant identifying the interactive update field in the result
// payload.
constexpr char kInteractiveUpdateFieldName[] = "interactiveUpdate";
// String constant identifying the user message field in the result payload.
constexpr char kUserMessageFieldName[] = "userMessage";

// String constant identifying the id field in the command payload.
constexpr char kIdFieldName[] = "id";
// String constant identifying the command field in the command payload.
constexpr char kCommandFieldName[] = "command";
// String constant identifying the include output field in the command payload.
constexpr char kIncludeOutputFieldName[] = "includeOutput";

template <typename T>
bool PopulateMojoEnumValueIfValid(int possible_enum, T* valid_enum_out) {
  DCHECK(valid_enum_out);
  if (!base::IsValueInRangeForNumericType<
          typename std::underlying_type<T>::type>(possible_enum)) {
    return false;
  }
  T enum_to_check = static_cast<T>(possible_enum);
  if (!ash::cros_healthd::mojom::IsKnownEnumValue(enum_to_check))
    return false;
  *valid_enum_out = enum_to_check;
  return true;
}

}  // namespace

class DeviceCommandGetRoutineUpdateJob::Payload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit Payload(ash::cros_healthd::mojom::RoutineUpdatePtr update);
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;
  ~Payload() override = default;

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  ash::cros_healthd::mojom::RoutineUpdatePtr update_;
};

DeviceCommandGetRoutineUpdateJob::Payload::Payload(
    ash::cros_healthd::mojom::RoutineUpdatePtr update)
    : update_(std::move(update)) {}

std::unique_ptr<std::string>
DeviceCommandGetRoutineUpdateJob::Payload::Serialize() {
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(kProgressPercentFieldName, update_->progress_percent);
  if (update_->output.is_valid()) {
    // TODO(crbug.com/1056323): Serialize update_->output. For now, set a dummy
    // value.
    root_dict.SetStringKey(kOutputFieldName, "Dummy");
  }

  const auto& routine_update_union = update_->routine_update_union;
  if (routine_update_union->is_noninteractive_update()) {
    const auto& noninteractive_update =
        routine_update_union->get_noninteractive_update();
    base::Value noninteractive_dict(base::Value::Type::DICTIONARY);
    noninteractive_dict.SetIntKey(
        kStatusFieldName, static_cast<int>(noninteractive_update->status));
    noninteractive_dict.SetStringKey(
        kStatusMessageFieldName,
        std::move(noninteractive_update->status_message));
    root_dict.SetPath(kNonInteractiveUpdateFieldName,
                      std::move(noninteractive_dict));
  } else if (routine_update_union->is_interactive_update()) {
    base::Value interactive_dict(base::Value::Type::DICTIONARY);
    interactive_dict.SetIntKey(
        kUserMessageFieldName,
        static_cast<int>(
            routine_update_union->get_interactive_update()->user_message));
    root_dict.SetPath(kInteractiveUpdateFieldName, std::move(interactive_dict));
  }

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return std::make_unique<std::string>(std::move(payload));
}

DeviceCommandGetRoutineUpdateJob::DeviceCommandGetRoutineUpdateJob()
    : routine_id_(ash::cros_healthd::mojom::kFailedToStartId),
      command_(
          ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kGetStatus),
      include_output_(false) {}

DeviceCommandGetRoutineUpdateJob::~DeviceCommandGetRoutineUpdateJob() = default;

em::RemoteCommand_Type DeviceCommandGetRoutineUpdateJob::GetType() const {
  return em::RemoteCommand_Type_DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE;
}

bool DeviceCommandGetRoutineUpdateJob::ParseCommandPayload(
    const std::string& command_payload) {
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root.has_value())
    return false;
  if (!root->is_dict())
    return false;

  // Make sure the command payload specified a valid integer for the routine ID.
  absl::optional<int> id = root->FindIntKey(kIdFieldName);
  if (!id.has_value())
    return false;
  routine_id_ = id.value();

  // Make sure the command payload specified a valid
  // DiagnosticRoutineCommandEnum.
  absl::optional<int> command_enum = root->FindIntKey(kCommandFieldName);
  if (!command_enum.has_value())
    return false;
  if (!PopulateMojoEnumValueIfValid(command_enum.value(), &command_)) {
    SYSLOG(ERROR) << "Unknown DiagnosticRoutineCommandEnum in command payload: "
                  << command_enum.value();
    return false;
  }

  // Make sure the command payload specified a boolean for include_output.
  absl::optional<bool> include_output =
      root->FindBoolKey(kIncludeOutputFieldName);
  if (!include_output.has_value())
    return false;
  include_output_ = include_output.value();

  return true;
}

void DeviceCommandGetRoutineUpdateJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  SYSLOG(INFO)
      << "Executing GetRoutineUpdate command with DiagnosticRoutineCommandEnum "
      << command_;

  ash::cros_healthd::ServiceConnection::GetInstance()->GetRoutineUpdate(
      routine_id_, command_, include_output_,
      base::BindOnce(
          &DeviceCommandGetRoutineUpdateJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
          std::move(failed_callback)));
}

void DeviceCommandGetRoutineUpdateJob::OnCrosHealthdResponseReceived(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    ash::cros_healthd::mojom::RoutineUpdatePtr update) {
  if (!update) {
    SYSLOG(ERROR) << "No RoutineUpdate received from cros_healthd.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback), nullptr));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(succeeded_callback),
                                std::make_unique<Payload>(std::move(update))));
}

}  // namespace policy
