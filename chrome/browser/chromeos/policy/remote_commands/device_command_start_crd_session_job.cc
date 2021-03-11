// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

// Job parameters fields:

// Job requires that UI was idle for at least this period of time
// to proceed. If absent / equal to 0, job will proceed regardless of user
// activity.
const char kIdlenessCutoffFieldName[] = "idlenessCutoffSec";

// Regulates if remote session should be terminated upon any local input event.
const char kTerminateUponInputFieldName[] = "terminateUponInput";

// Result payload fields:

// Integer value containing DeviceCommandStartCRDSessionJob::ResultCode
const char kResultCodeFieldName[] = "resultCode";

// CRD Access Code if job was completed successfully
const char kResultAccessCodeFieldName[] = "accessCode";

// Optional detailed error message for error result codes.
const char kResultMessageFieldName[] = "message";

// Period in seconds since last user activity, if job finished with
// FAILURE_NOT_IDLE result code.
const char kResultLastActivityFieldName[] = "lastActivitySec";

}  // namespace

class DeviceCommandStartCRDSessionJob::ResultPayload
    : public RemoteCommandJob::ResultPayload {
 public:
  ResultPayload(ResultCode result_code,
                const base::Optional<std::string>& access_code,
                const base::Optional<base::TimeDelta>& time_delta,
                const base::Optional<std::string>& error_message);
  ~ResultPayload() override {}

  static std::unique_ptr<ResultPayload> CreateSuccessPayload(
      const std::string& access_code);
  static std::unique_ptr<ResultPayload> CreateNonIdlePayload(
      const base::TimeDelta& time_delta);
  static std::unique_ptr<ResultPayload> CreateErrorPayload(
      ResultCode result_code,
      const std::string& error_message);

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  std::string payload_;
};

DeviceCommandStartCRDSessionJob::ResultPayload::ResultPayload(
    ResultCode result_code,
    const base::Optional<std::string>& access_code,
    const base::Optional<base::TimeDelta>& time_delta,
    const base::Optional<std::string>& error_message) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kResultCodeFieldName, base::Value(result_code));
  if (error_message && !error_message.value().empty())
    value.SetKey(kResultMessageFieldName, base::Value(error_message.value()));
  if (access_code)
    value.SetKey(kResultAccessCodeFieldName, base::Value(access_code.value()));
  if (time_delta) {
    value.SetKey(kResultLastActivityFieldName,
                 base::Value(static_cast<int>(time_delta.value().InSeconds())));
  }
  base::JSONWriter::Write(value, &payload_);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateSuccessPayload(
    const std::string& access_code) {
  return std::make_unique<ResultPayload>(ResultCode::SUCCESS, access_code,
                                         base::nullopt /*time_delta*/,
                                         base::nullopt /* error_message */);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateNonIdlePayload(
    const base::TimeDelta& time_delta) {
  return std::make_unique<ResultPayload>(
      ResultCode::FAILURE_NOT_IDLE, base::nullopt /* access_code */, time_delta,
      base::nullopt /* error_message */);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  DCHECK(result_code != ResultCode::FAILURE_NOT_IDLE);
  return std::make_unique<ResultPayload>(
      result_code, base::nullopt /* access_code */,
      base::nullopt /*time_delta*/, error_message);
}

std::unique_ptr<std::string>
DeviceCommandStartCRDSessionJob::ResultPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

DeviceCommandStartCRDSessionJob::DeviceCommandStartCRDSessionJob(
    Delegate* crd_host_delegate)
    : delegate_(crd_host_delegate), terminate_session_attemtpted_(false) {}

DeviceCommandStartCRDSessionJob::~DeviceCommandStartCRDSessionJob() {}

enterprise_management::RemoteCommand_Type
DeviceCommandStartCRDSessionJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_DEVICE_START_CRD_SESSION;
}

bool DeviceCommandStartCRDSessionJob::ParseCommandPayload(
    const std::string& command_payload) {
  base::Optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;
  if (!root->is_dict())
    return false;

  base::Value* idleness_cutoff_value =
      root->FindKeyOfType(kIdlenessCutoffFieldName, base::Value::Type::INTEGER);
  if (idleness_cutoff_value) {
    idleness_cutoff_ =
        base::TimeDelta::FromSeconds(idleness_cutoff_value->GetInt());
  } else {
    idleness_cutoff_ = base::TimeDelta::FromSeconds(0);
  }

  base::Value* terminate_upon_input_value = root->FindKeyOfType(
      kTerminateUponInputFieldName, base::Value::Type::BOOLEAN);
  if (terminate_upon_input_value) {
    terminate_upon_input_ = terminate_upon_input_value->GetBool();
  } else {
    terminate_upon_input_ = false;
  }

  return true;
}

void DeviceCommandStartCRDSessionJob::FinishWithError(
    const ResultCode result_code,
    const std::string& message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  if (!failed_callback_)
    return;  // Task was terminated.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(failed_callback_),
                     ResultPayload::CreateErrorPayload(result_code, message)));
}

void DeviceCommandStartCRDSessionJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  VLOG(0) << "Running start crd session command";

  if (delegate_->HasActiveSession()) {
    CHECK(!terminate_session_attemtpted_);
    terminate_session_attemtpted_ = true;
    delegate_->TerminateSession(base::BindOnce(
        &DeviceCommandStartCRDSessionJob::RunImpl, weak_factory_.GetWeakPtr(),
        std::move(succeeded_callback), std::move(failed_callback)));
    return;
  }

  terminate_session_attemtpted_ = false;
  failed_callback_ = std::move(failed_callback);
  succeeded_callback_ = std::move(succeeded_callback);

  if (!delegate_->AreServicesReady()) {
    FinishWithError(ResultCode::FAILURE_SERVICES_NOT_READY, "");
    return;
  }

  if (!delegate_->IsRunningKiosk()) {
    FinishWithError(ResultCode::FAILURE_NOT_A_KIOSK, "");
    return;
  }

  bool device_is_idle = delegate_->GetIdlenessPeriod() >= idleness_cutoff_;

  if (!device_is_idle) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                  ResultPayload::CreateNonIdlePayload(
                                      delegate_->GetIdlenessPeriod())));
    return;
  }

  delegate_->FetchOAuthToken(
      base::BindOnce(&DeviceCommandStartCRDSessionJob::OnOAuthTokenReceived,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceCommandStartCRDSessionJob::FinishWithError,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCRDSessionJob::OnOAuthTokenReceived(
    const std::string& token) {
  oauth_token_ = token;
  delegate_->StartCRDHostAndGetCode(
      oauth_token_, terminate_upon_input_,
      base::BindOnce(&DeviceCommandStartCRDSessionJob::OnAccessCodeReceived,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceCommandStartCRDSessionJob::FinishWithError,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCRDSessionJob::OnAccessCodeReceived(
    const std::string& access_code) {
  if (!succeeded_callback_)
    return;  // Task was terminated.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(succeeded_callback_),
                     ResultPayload::CreateSuccessPayload(access_code)));
}

void DeviceCommandStartCRDSessionJob::TerminateImpl() {
  succeeded_callback_.Reset();
  failed_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->TerminateSession(base::OnceClosure());
}

}  // namespace policy
