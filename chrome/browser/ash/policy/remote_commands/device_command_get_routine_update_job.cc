// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

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
  if (!ash::cros_healthd::mojom::IsKnownEnumValue(enum_to_check)) {
    return false;
  }
  *valid_enum_out = enum_to_check;
  return true;
}

std::string CreatePayload(ash::cros_healthd::mojom::RoutineUpdatePtr update) {
  auto root_dict = base::Value::Dict().Set(
      kProgressPercentFieldName, static_cast<int>(update->progress_percent));
  if (update->output.is_valid()) {
    // TODO(crbug.com/1056323): Serialize update->output. For now, set a dummy
    // value.
    root_dict.Set(kOutputFieldName, "Dummy");
  }

  const auto& routine_update_union = update->routine_update_union;
  if (routine_update_union->is_noninteractive_update()) {
    const auto& noninteractive_update =
        routine_update_union->get_noninteractive_update();
    auto noninteractive_dict =
        base::Value::Dict()
            .Set(kStatusFieldName,
                 static_cast<int>(noninteractive_update->status))
            .Set(kStatusMessageFieldName,
                 std::move(noninteractive_update->status_message));
    root_dict.Set(kNonInteractiveUpdateFieldName,
                  std::move(noninteractive_dict));
  } else if (routine_update_union->is_interactive_update()) {
    auto interactive_dict = base::Value::Dict().Set(
        kUserMessageFieldName,
        static_cast<int>(
            routine_update_union->get_interactive_update()->user_message));
    root_dict.Set(kInteractiveUpdateFieldName, std::move(interactive_dict));
  }

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

}  // namespace

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
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root.has_value()) {
    return false;
  }
  if (!root->is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = root->GetDict();
  // Make sure the command payload specified a valid integer for the routine ID.
  std::optional<int> id = dict.FindInt(kIdFieldName);
  if (!id.has_value()) {
    return false;
  }
  routine_id_ = id.value();

  // Make sure the command payload specified a valid
  // DiagnosticRoutineCommandEnum.
  std::optional<int> command_enum = dict.FindInt(kCommandFieldName);
  if (!command_enum.has_value()) {
    return false;
  }
  if (!PopulateMojoEnumValueIfValid(command_enum.value(), &command_)) {
    SYSLOG(ERROR) << "Unknown DiagnosticRoutineCommandEnum in command payload: "
                  << command_enum.value();
    return false;
  }

  // Make sure the command payload specified a boolean for include_output.
  std::optional<bool> include_output = dict.FindBool(kIncludeOutputFieldName);
  if (!include_output.has_value()) {
    return false;
  }
  include_output_ = include_output.value();

  return true;
}

void DeviceCommandGetRoutineUpdateJob::RunImpl(
    CallbackWithResult result_callback) {
  SYSLOG(INFO)
      << "Executing GetRoutineUpdate command with DiagnosticRoutineCommandEnum "
      << command_;

  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetDiagnosticsService()
      ->GetRoutineUpdate(
          routine_id_, command_, include_output_,
          base::BindOnce(
              &DeviceCommandGetRoutineUpdateJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void DeviceCommandGetRoutineUpdateJob::OnCrosHealthdResponseReceived(
    CallbackWithResult result_callback,
    ash::cros_healthd::mojom::RoutineUpdatePtr update) {
  if (!update) {
    SYSLOG(ERROR) << "No RoutineUpdate received from cros_healthd.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::nullopt));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), ResultType::kSuccess,
                     CreatePayload(std::move(update))));
}

}  // namespace policy
