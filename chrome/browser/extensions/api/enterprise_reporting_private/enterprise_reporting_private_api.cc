// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace extensions {

namespace enterprise_reporting {

const char kDeviceIdNotFound[] = "Failed to retrieve the device id.";
const char kEndpointVerificationRetrievalFailed[] =
    "Failed to retrieve the endpoint verification data.";
const char kEndpointVerificationStoreFailed[] =
    "Failed to store the endpoint verification data.";
const char kEndpointVerificationSecretRetrievalFailed[] = "%ld";

}  // namespace enterprise_reporting

// GetDeviceId

EnterpriseReportingPrivateGetDeviceIdFunction::
    EnterpriseReportingPrivateGetDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceIdFunction::Run() {
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id.empty())
    return RespondNow(Error(enterprise_reporting::kDeviceIdNotFound));
  return RespondNow(OneArgument(std::make_unique<base::Value>(client_id)));
}

EnterpriseReportingPrivateGetDeviceIdFunction::
    ~EnterpriseReportingPrivateGetDeviceIdFunction() = default;

// getPersistentSecret

EnterpriseReportingPrivateGetPersistentSecretFunction::
    EnterpriseReportingPrivateGetPersistentSecretFunction() = default;
EnterpriseReportingPrivateGetPersistentSecretFunction::
    ~EnterpriseReportingPrivateGetPersistentSecretFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetPersistentSecretFunction::Run() {
  std::unique_ptr<
      api::enterprise_reporting_private::GetPersistentSecret::Params>
      params(api::enterprise_reporting_private::GetPersistentSecret::Params::
                 Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool force_create = params->reset_secret ? *params->reset_secret : false;
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceSecret, force_create,
          base::BindOnce(
              &EnterpriseReportingPrivateGetPersistentSecretFunction::
                  OnDataRetrieved,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::OnDataRetrieved(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& data,
    long int status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse,
          this, data, status));
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse(
    const std::string& data,
    long int status) {
  if (status == 0) {  // Success.
    VLOG(1) << "The Endpoint Verification secret was retrieved.";
    Respond(OneArgument(std::make_unique<base::Value>(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
  } else {
    VLOG(1) << "Endpoint Verification secret retrieval error: " << status;
    Respond(Error(base::StringPrintf(
        enterprise_reporting::kEndpointVerificationSecretRetrievalFailed,
        static_cast<long int>(status))));
  }
}

// getDeviceData

EnterpriseReportingPrivateGetDeviceDataFunction::
    EnterpriseReportingPrivateGetDeviceDataFunction() = default;
EnterpriseReportingPrivateGetDeviceDataFunction::
    ~EnterpriseReportingPrivateGetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::GetDeviceData::Params>
      params(api::enterprise_reporting_private::GetDeviceData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceData, params->id,
          base::BindOnce(
              &EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& data,
    RetrieveDeviceDataStatus status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateGetDeviceDataFunction::SendResponse, this,
          data, status));
}

void EnterpriseReportingPrivateGetDeviceDataFunction::SendResponse(
    const std::string& data,
    RetrieveDeviceDataStatus status) {
  switch (status) {
    case RetrieveDeviceDataStatus::kSuccess:
      VLOG(1) << "The Endpoint Verification data was retrieved.";
      Respond(
          OneArgument(std::make_unique<base::Value>(base::Value::BlobStorage(
              reinterpret_cast<const uint8_t*>(data.data()),
              reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
      return;
    case RetrieveDeviceDataStatus::kDataRecordNotFound:
      VLOG(1) << "The Endpoint Verification data is not present.";
      Respond(NoArguments());
      return;
    default:
      VLOG(1) << "Endpoint Verification data retrieval error: "
              << static_cast<long int>(status);
      Respond(
          Error(enterprise_reporting::kEndpointVerificationRetrievalFailed));
  }
}

// setDeviceData

EnterpriseReportingPrivateSetDeviceDataFunction::
    EnterpriseReportingPrivateSetDeviceDataFunction() = default;
EnterpriseReportingPrivateSetDeviceDataFunction::
    ~EnterpriseReportingPrivateSetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateSetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::SetDeviceData::Params>
      params(api::enterprise_reporting_private::SetDeviceData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &StoreDeviceData, params->id, std::move(params->data),
          base::BindOnce(
              &EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateSetDeviceDataFunction::SendResponse, this,
          status));
}

void EnterpriseReportingPrivateSetDeviceDataFunction::SendResponse(
    bool status) {
  if (status) {
    VLOG(1) << "The Endpoint Verification data was stored.";
    Respond(NoArguments());
  } else {
    VLOG(1) << "Endpoint Verification data storage error.";
    Respond(Error(enterprise_reporting::kEndpointVerificationStoreFailed));
  }
}

// getDeviceInfo

EnterpriseReportingPrivateGetDeviceInfoFunction::
    EnterpriseReportingPrivateGetDeviceInfoFunction() = default;
EnterpriseReportingPrivateGetDeviceInfoFunction::
    ~EnterpriseReportingPrivateGetDeviceInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceInfoFunction::Run() {
#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&enterprise_reporting::DeviceInfoFetcher::Fetch,
                     enterprise_reporting::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#else
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateTaskRunner({base::MayBlock()}).get(), FROM_HERE,
      base::BindOnce(&enterprise_reporting::DeviceInfoFetcher::Fetch,
                     enterprise_reporting::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#endif  // defined(OS_WIN)

  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceInfoFunction::OnDeviceInfoRetrieved(
    const api::enterprise_reporting_private::DeviceInfo& device_info) {
  Respond(OneArgument(device_info.ToValue()));
}

}  // namespace extensions
