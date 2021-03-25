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
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace extensions {

namespace {
#if !defined(OS_CHROMEOS)
const char kEndpointVerificationRetrievalFailed[] =
    "Failed to retrieve the endpoint verification data.";
const char kEndpointVerificationStoreFailed[] =
    "Failed to store the endpoint verification data.";

api::enterprise_reporting_private::SettingValue ToInfoSettingValue(
    enterprise_signals::DeviceInfo::SettingValue value) {
  using SettingValue = enterprise_signals::DeviceInfo::SettingValue;
  switch (value) {
    case SettingValue::NONE:
      return api::enterprise_reporting_private::SETTING_VALUE_NONE;
    case SettingValue::UNKNOWN:
      return api::enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
    case SettingValue::DISABLED:
      return api::enterprise_reporting_private::SETTING_VALUE_DISABLED;
    case SettingValue::ENABLED:
      return api::enterprise_reporting_private::SETTING_VALUE_ENABLED;
  }
}

api::enterprise_reporting_private::DeviceInfo ToDeviceInfo(
    const enterprise_signals::DeviceInfo& device_signals) {
  api::enterprise_reporting_private::DeviceInfo device_info;

  device_info.os_name = device_signals.os_name;
  device_info.os_version = device_signals.os_version;
  device_info.device_host_name = device_signals.device_host_name;
  device_info.device_model = device_signals.device_model;
  device_info.serial_number = device_signals.serial_number;
  device_info.screen_lock_secured =
      ToInfoSettingValue(device_signals.screen_lock_secured);
  device_info.disk_encrypted =
      ToInfoSettingValue(device_signals.disk_encrypted);

  return device_info;
}
#endif  // !defined(OS_CHROMEOS)

api::enterprise_reporting_private::ContextInfo ToContextInfo(
    const enterprise_signals::ContextInfo& signals) {
  api::enterprise_reporting_private::ContextInfo info;

  info.browser_affiliation_ids = signals.browser_affiliation_ids;
  info.profile_affiliation_ids = signals.profile_affiliation_ids;
  info.on_file_attached_providers = signals.on_file_attached_providers;
  info.on_file_downloaded_providers = signals.on_file_downloaded_providers;
  info.on_bulk_data_entry_providers = signals.on_bulk_data_entry_providers;
  info.on_security_event_providers = signals.on_security_event_providers;
  switch (signals.realtime_url_check_mode) {
    case safe_browsing::REAL_TIME_CHECK_DISABLED:
      info.realtime_url_check_mode = extensions::api::
          enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED;
      break;
    case safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED:
      info.realtime_url_check_mode =
          extensions::api::enterprise_reporting_private::
              REALTIME_URL_CHECK_MODE_ENABLED_MAIN_FRAME;
      break;
  }
  info.browser_version = signals.browser_version;

  return info;
}

}  // namespace

#if !defined(OS_CHROMEOS)
namespace enterprise_reporting {
const char kDeviceIdNotFound[] = "Failed to retrieve the device id.";
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
  return RespondNow(OneArgument(base::Value(client_id)));
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
    Respond(OneArgument(base::Value(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
  } else {
    VLOG(1) << "Endpoint Verification secret retrieval error: " << status;
    Respond(Error(base::StringPrintf("%ld", static_cast<long int>(status))));
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
      Respond(OneArgument(base::Value(base::Value::BlobStorage(
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
      Respond(Error(kEndpointVerificationRetrievalFailed));
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
    Respond(Error(kEndpointVerificationStoreFailed));
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
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#else
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateTaskRunner({base::MayBlock()}).get(), FROM_HERE,
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#endif  // defined(OS_WIN)

  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceInfoFunction::OnDeviceInfoRetrieved(
    const enterprise_signals::DeviceInfo& device_signals) {
  Respond(OneArgument(
      base::Value::FromUniquePtrValue(ToDeviceInfo(device_signals).ToValue())));
}

#endif  // !defined(OS_CHROMEOS)

// getContextInfo

EnterpriseReportingPrivateGetContextInfoFunction::
    EnterpriseReportingPrivateGetContextInfoFunction() = default;
EnterpriseReportingPrivateGetContextInfoFunction::
    ~EnterpriseReportingPrivateGetContextInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetContextInfoFunction::Run() {
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetInstance()
          ->GetForBrowserContext(browser_context());
  DCHECK(connectors_service);

  context_info_fetcher_ =
      enterprise_signals::ContextInfoFetcher::CreateInstance(
          browser_context(), connectors_service);
  context_info_fetcher_->Fetch(base::BindOnce(
      &EnterpriseReportingPrivateGetContextInfoFunction::OnContextInfoRetrieved,
      this));

  return RespondLater();
}

void EnterpriseReportingPrivateGetContextInfoFunction::OnContextInfoRetrieved(
    enterprise_signals::ContextInfo context_info) {
  Respond(OneArgument(
      base::Value::FromUniquePtrValue(ToContextInfo(context_info).ToValue())));
}

}  // namespace extensions
