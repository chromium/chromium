// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/dm_token_utils.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/statusor.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include <optional>

#include "base/strings/string_util.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/conversion_utils.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/common/signals_features.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#include "components/content_settings/core/common/pref_names.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "net/cert/x509_util.h"

namespace extensions {

namespace {
#if !BUILDFLAG(IS_CHROMEOS)
const char kEndpointVerificationRetrievalFailed[] =
    "Failed to retrieve the endpoint verification data.";
const char kEndpointVerificationStoreFailed[] =
    "Failed to store the endpoint verification data.";
#endif  // !BUILDFLAG(IS_CHROMEOS)

api::enterprise_reporting_private::SettingValue ToInfoSettingValue(
    enterprise_signals::SettingValue value) {
  switch (value) {
    case enterprise_signals::SettingValue::UNKNOWN:
      return api::enterprise_reporting_private::SettingValue::kUnknown;
    case enterprise_signals::SettingValue::DISABLED:
      return api::enterprise_reporting_private::SettingValue::kDisabled;
    case enterprise_signals::SettingValue::ENABLED:
      return api::enterprise_reporting_private::SettingValue::kEnabled;
  }
}

api::enterprise_reporting_private::ContextInfo ToContextInfo(
    enterprise_signals::ContextInfo&& signals) {
  api::enterprise_reporting_private::ContextInfo info;

  info.browser_affiliation_ids = std::move(signals.browser_affiliation_ids);
  info.profile_affiliation_ids = std::move(signals.profile_affiliation_ids);
  info.on_file_attached_providers =
      std::move(signals.on_file_attached_providers);
  info.on_file_downloaded_providers =
      std::move(signals.on_file_downloaded_providers);
  info.on_bulk_data_entry_providers =
      std::move(signals.on_bulk_data_entry_providers);
  info.on_print_providers = std::move(signals.on_print_providers);
  info.on_security_event_providers =
      std::move(signals.on_security_event_providers);
  info.site_isolation_enabled = signals.site_isolation_enabled;
  info.chrome_remote_desktop_app_blocked =
      signals.chrome_remote_desktop_app_blocked;
  info.third_party_blocking_enabled = signals.third_party_blocking_enabled;
  info.os_firewall = ToInfoSettingValue(signals.os_firewall);
  info.system_dns_servers = std::move(signals.system_dns_servers);
  switch (signals.realtime_url_check_mode) {
    case enterprise_connectors::REAL_TIME_CHECK_DISABLED:
      info.realtime_url_check_mode = extensions::api::
          enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled;
      break;
    case enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED:
      info.realtime_url_check_mode = extensions::api::
          enterprise_reporting_private::RealtimeUrlCheckMode::kEnabledMainFrame;
      break;
  }
  info.browser_version = std::move(signals.browser_version);
  info.built_in_dns_client_enabled = signals.built_in_dns_client_enabled;
  info.enterprise_profile_id = signals.enterprise_profile_id;

  switch (signals.safe_browsing_protection_level) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SafeBrowsingLevel::kDisabled;
      break;
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SafeBrowsingLevel::kStandard;
      break;
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SafeBrowsingLevel::kEnhanced;
      break;
  }
  if (!signals.password_protection_warning_trigger.has_value()) {
    info.password_protection_warning_trigger = extensions::api::
        enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset;
  } else {
    switch (signals.password_protection_warning_trigger.value()) {
      case safe_browsing::PASSWORD_PROTECTION_OFF:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PasswordProtectionTrigger::kPasswordProtectionOff;
        break;
      case safe_browsing::PASSWORD_REUSE:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PasswordProtectionTrigger::kPasswordReuse;
        break;
      case safe_browsing::PHISHING_REUSE:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PasswordProtectionTrigger::kPhishingReuse;
        break;
      case safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  return info;
}

bool AllowClientCertificateReportingForUsers() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return base::FeatureList::IsEnabled(
      enterprise_signals::features::kAllowClientCertificateReportingForUsers);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

bool IsProfilePrefManaged(Profile* profile, std::string_view pref_name) {
  const auto* pref = profile->GetPrefs()->FindPreference(pref_name);
  return pref && pref->IsManaged();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

device_signals::SignalsAggregationRequest CreateAggregationRequest(
    device_signals::SignalName signal_name) {
  device_signals::SignalsAggregationRequest request;
  request.signal_names.emplace(signal_name);
  return request;
}

void StartSignalCollection(
    const std::string& user_id,
    device_signals::SignalsAggregationRequest request,
    content::BrowserContext* browser_context,
    base::OnceCallback<void(device_signals::SignalsAggregationResponse)>
        callback) {
  DCHECK(browser_context);
  auto* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  auto* signals_aggregator =
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile);
  DCHECK(signals_aggregator);

  device_signals::UserContext user_context;
  user_context.user_id = user_id;

  signals_aggregator->GetSignalsForUser(
      std::move(user_context), std::move(request), std::move(callback));
}

bool CanReturnResponse(content::BrowserContext* browser_context) {
  return browser_context && !browser_context->ShutdownStarted();
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS)
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
  if (client_id.empty()) {
    return RespondNow(Error(enterprise_reporting::kDeviceIdNotFound));
  }
  return RespondNow(WithArguments(client_id));
}

EnterpriseReportingPrivateGetDeviceIdFunction::
    ~EnterpriseReportingPrivateGetDeviceIdFunction() = default;

// getPersistentSecret

#if !BUILDFLAG(IS_LINUX)

EnterpriseReportingPrivateGetPersistentSecretFunction::
    EnterpriseReportingPrivateGetPersistentSecretFunction() = default;
EnterpriseReportingPrivateGetPersistentSecretFunction::
    ~EnterpriseReportingPrivateGetPersistentSecretFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetPersistentSecretFunction::Run() {
  std::optional<api::enterprise_reporting_private::GetPersistentSecret::Params>
      params = api::enterprise_reporting_private::GetPersistentSecret::Params::
          Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  bool force_create = params->reset_secret ? *params->reset_secret : false;
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceSecret, force_create,
          base::BindOnce(
              &EnterpriseReportingPrivateGetPersistentSecretFunction::
                  OnDataRetrieved,
              this, base::SingleThreadTaskRunner::GetCurrentDefault())));
  return RespondLater();
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::OnDataRetrieved(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& data,
    int32_t status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse,
          this, data, status));
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse(
    const std::string& data,
    int32_t status) {
  if (status == 0) {  // Success.
    VLOG(1) << "The Endpoint Verification secret was retrieved.";
    Respond(WithArguments(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size()))));
  } else {
    VLOG(1) << "Endpoint Verification secret retrieval error: " << status;
    Respond(Error(base::StringPrintf("%d", status)));
  }
}

#endif  // !BUILDFLAG(IS_LINUX)

// getDeviceData

EnterpriseReportingPrivateGetDeviceDataFunction::
    EnterpriseReportingPrivateGetDeviceDataFunction() = default;
EnterpriseReportingPrivateGetDeviceDataFunction::
    ~EnterpriseReportingPrivateGetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceDataFunction::Run() {
  std::optional<api::enterprise_reporting_private::GetDeviceData::Params>
      params = api::enterprise_reporting_private::GetDeviceData::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceData, params->id,
          base::BindOnce(
              &EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved,
              this, base::SingleThreadTaskRunner::GetCurrentDefault())));
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
      Respond(WithArguments(base::Value::BlobStorage(
          reinterpret_cast<const uint8_t*>(data.data()),
          reinterpret_cast<const uint8_t*>(data.data() + data.size()))));
      return;
    case RetrieveDeviceDataStatus::kDataRecordNotFound:
      VLOG(1) << "The Endpoint Verification data is not present.";
      Respond(WithArguments(base::Value::BlobStorage()));
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
  std::optional<api::enterprise_reporting_private::SetDeviceData::Params>
      params = api::enterprise_reporting_private::SetDeviceData::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &StoreDeviceData, params->id, std::move(params->data),
          base::BindOnce(
              &EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored,
              this, base::SingleThreadTaskRunner::GetCurrentDefault())));
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

// static
api::enterprise_reporting_private::DeviceInfo
EnterpriseReportingPrivateGetDeviceInfoFunction::ToDeviceInfo(
    const enterprise_signals::DeviceInfo& device_signals) {
  api::enterprise_reporting_private::DeviceInfo device_info;

  device_info.os_name = device_signals.os_name;
  device_info.os_version = device_signals.os_version;
  device_info.security_patch_level = device_signals.security_patch_level;
  device_info.device_host_name = device_signals.device_host_name;
  device_info.device_model = device_signals.device_model;
  device_info.serial_number = device_signals.serial_number;
  device_info.screen_lock_secured =
      ToInfoSettingValue(device_signals.screen_lock_secured);
  device_info.disk_encrypted =
      ToInfoSettingValue(device_signals.disk_encrypted);
  device_info.mac_addresses = device_signals.mac_addresses;
  device_info.windows_machine_domain = device_signals.windows_machine_domain;
  device_info.windows_user_domain = device_signals.windows_user_domain;
  if (device_signals.secure_boot_enabled.has_value()) {
    device_info.secure_boot_enabled =
        ToInfoSettingValue(device_signals.secure_boot_enabled.value());
  }

  return device_info;
}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceInfoFunction::Run() {
#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#else
  base::ThreadPool::CreateTaskRunner({base::MayBlock()})
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &enterprise_signals::DeviceInfoFetcher::Fetch,
              enterprise_signals::DeviceInfoFetcher::CreateInstance()),
          base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                             OnDeviceInfoRetrieved,
                         this));
#endif  // BUILDFLAG(IS_WIN)

  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceInfoFunction::OnDeviceInfoRetrieved(
    const enterprise_signals::DeviceInfo& device_signals) {
  Respond(WithArguments(ToDeviceInfo(device_signals).ToValue()));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  Respond(WithArguments(ToContextInfo(std::move(context_info)).ToValue()));
}

// getCertificate

EnterpriseReportingPrivateGetCertificateFunction::
    EnterpriseReportingPrivateGetCertificateFunction() = default;
EnterpriseReportingPrivateGetCertificateFunction::
    ~EnterpriseReportingPrivateGetCertificateFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetCertificateFunction::Run() {
  std::optional<api::enterprise_reporting_private::GetCertificate::Params>
      params =
          api::enterprise_reporting_private::GetCertificate::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* profile = Profile::FromBrowserContext(browser_context());
  if (AllowClientCertificateReportingForUsers()) {
    if (!IsProfilePrefManaged(profile,
                              prefs::kManagedAutoSelectCertificateForUrls)) {
      // If the policy is not set, then fail fast as the policy is required to
      // select which certificate to report.
      api::enterprise_reporting_private::Certificate ret;
      ret.status = extensions::api::enterprise_reporting_private::
          CertificateStatus::kPolicyUnset;
      return RespondNow(WithArguments(ret.ToValue()));
    }
  } else if (!enterprise_util::IsMachinePolicyPref(
                 prefs::kManagedAutoSelectCertificateForUrls)) {
    // If certificate reporting is not enabled for the user and
    // AutoSelectCertificateForUrl is not set at the machine level, this
    // operation is not supported and should return immediately with the
    // appropriate status field value.
    api::enterprise_reporting_private::Certificate ret;
    ret.status = extensions::api::enterprise_reporting_private::
        CertificateStatus::kPolicyUnset;
    return RespondNow(WithArguments(ret.ToValue()));
  }

  client_cert_fetcher_ =
      enterprise_signals::ClientCertificateFetcher::Create(browser_context());
  client_cert_fetcher_->FetchAutoSelectedCertificateForUrl(
      GURL(params->url),
      base::BindOnce(&EnterpriseReportingPrivateGetCertificateFunction::
                         OnClientCertFetched,
                     this));

  return RespondLater();
}

void EnterpriseReportingPrivateGetCertificateFunction::OnClientCertFetched(
    std::unique_ptr<net::ClientCertIdentity> cert) {
  api::enterprise_reporting_private::Certificate ret;

  // Getting here means the status is always OK, but the |encoded_certificate|
  // field is only set if there actually was a certificate selected.
  ret.status =
      extensions::api::enterprise_reporting_private::CertificateStatus::kOk;
  if (cert) {
    std::string_view der_cert = net::x509_util::CryptoBufferAsStringPiece(
        cert->certificate()->cert_buffer());
    ret.encoded_certificate.emplace(der_cert.begin(), der_cert.end());
  }

  Respond(WithArguments(ret.ToValue()));
}

#if BUILDFLAG(IS_CHROMEOS)

// enqueueRecord

EnterpriseReportingPrivateEnqueueRecordFunction::
    EnterpriseReportingPrivateEnqueueRecordFunction() = default;

EnterpriseReportingPrivateEnqueueRecordFunction::
    ~EnterpriseReportingPrivateEnqueueRecordFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateEnqueueRecordFunction::Run() {
  auto* profile = Profile::FromBrowserContext(browser_context());
  DCHECK(profile);

  if (!IsProfileAffiliated(profile)) {
    return RespondNow(Error(kErrorProfileNotAffiliated));
  }

  std::optional<api::enterprise_reporting_private::EnqueueRecord::Params>
      params = api::enterprise_reporting_private::EnqueueRecord::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Parse params
  const auto event_type = params->request.event_type;
  ::reporting::Record record;
  ::reporting::Priority priority;
  if (!TryParseParams(std::move(params), record, priority)) {
    return RespondNow(Error(kErrorInvalidEnqueueRecordRequest));
  }

  // Attach appropriate DM token to record
  if (!TryAttachDMTokenToRecord(record, event_type)) {
    return RespondNow(Error(kErrorCannotAssociateRecordWithUser));
  }

  // Initiate enqueue and subsequent upload
  auto enqueue_completion_cb = base::BindOnce(
      &EnterpriseReportingPrivateEnqueueRecordFunction::OnRecordEnqueued, this);
  auto* reporting_client = ::chromeos::MissiveClient::Get();
  DCHECK(reporting_client);
  reporting_client->EnqueueRecord(priority, record,
                                  std::move(enqueue_completion_cb));
  return RespondLater();
}

bool EnterpriseReportingPrivateEnqueueRecordFunction::TryParseParams(
    std::optional<api::enterprise_reporting_private::EnqueueRecord::Params>
        params,
    ::reporting::Record& record,
    ::reporting::Priority& priority) {
  if (params->request.record_data.empty()) {
    return false;
  }

  const auto* record_data =
      reinterpret_cast<const char*>(params->request.record_data.data());
  if (!record.ParseFromArray(record_data, params->request.record_data.size())) {
    // Invalid record payload
    return false;
  }

  if (!record.has_timestamp_us()) {
    // Missing record timestamp
    return false;
  }

  if (!::reporting::Priority_IsValid(params->request.priority) ||
      !::reporting::Priority_Parse(
          ::reporting::Priority_Name(params->request.priority), &priority)) {
    // Invalid priority
    return false;
  }

  // Valid
  return true;
}

bool EnterpriseReportingPrivateEnqueueRecordFunction::TryAttachDMTokenToRecord(
    ::reporting::Record& record,
    api::enterprise_reporting_private::EventType event_type) {
  if (event_type == api::enterprise_reporting_private::EventType::kDevice) {
    // Device DM tokens are automatically appended during uploads, so we need
    // not specify them with the record.
    return true;
  }

  auto* profile = Profile::FromBrowserContext(browser_context());

  const policy::DMToken& dm_token = policy::GetDMToken(profile);
  if (!dm_token.is_valid()) {
    return false;
  }

  record.set_dm_token(dm_token.value());
  return true;
}

void EnterpriseReportingPrivateEnqueueRecordFunction::OnRecordEnqueued(
    ::reporting::Status result) {
  if (!result.ok()) {
    Respond(Error(kUnexpectedErrorEnqueueRecordRequest));
    return;
  }

  Respond(NoArguments());
}

bool EnterpriseReportingPrivateEnqueueRecordFunction::IsProfileAffiliated(
    Profile* profile) {
  if (profile_is_affiliated_for_testing_) {
    return true;
  }
  return enterprise_util::IsProfileAffiliated(profile);
}

void EnterpriseReportingPrivateEnqueueRecordFunction::
    SetProfileIsAffiliatedForTesting(bool is_affiliated) {
  profile_is_affiliated_for_testing_ = is_affiliated;
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// getFileSystemInfo

EnterpriseReportingPrivateGetFileSystemInfoFunction::
    EnterpriseReportingPrivateGetFileSystemInfoFunction() = default;
EnterpriseReportingPrivateGetFileSystemInfoFunction::
    ~EnterpriseReportingPrivateGetFileSystemInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetFileSystemInfoFunction::Run() {
  if (!IsNewFunctionEnabled(
          enterprise_signals::features::NewEvFunction::kFileSystemInfo)) {
    return RespondNow(Error(device_signals::ErrorToString(
        device_signals::SignalCollectionError::kUnsupported)));
  }

  std::optional<api::enterprise_reporting_private::GetFileSystemInfo::Params>
      params =
          api::enterprise_reporting_private::GetFileSystemInfo::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Verify that all file paths are UTF8.
  bool paths_are_all_utf8 = true;
  for (const auto& api_options_param : params->request.options) {
    if (!base::IsStringUTF8(api_options_param.path)) {
      paths_are_all_utf8 = false;
      break;
    }
  }
  EXTENSION_FUNCTION_VALIDATE(paths_are_all_utf8);

  auto aggregation_request = CreateAggregationRequest(signal_name());
  aggregation_request.file_system_signal_parameters =
      ConvertFileSystemInfoOptions(params->request.options);

  const size_t number_of_items =
      aggregation_request.file_system_signal_parameters.size();
  LogSignalCollectionRequestedWithItems(signal_name(), number_of_items);

  StartSignalCollection(
      params->request.user_context.user_id, aggregation_request,
      browser_context(),
      base::BindOnce(&EnterpriseReportingPrivateGetFileSystemInfoFunction::
                         OnSignalRetrieved,
                     this, base::TimeTicks::Now(), number_of_items));

  return RespondLater();
}

void EnterpriseReportingPrivateGetFileSystemInfoFunction::OnSignalRetrieved(
    base::TimeTicks start_time,
    size_t request_items_count,
    device_signals::SignalsAggregationResponse response) {
  if (!CanReturnResponse(browser_context())) {
    // The browser is no longer accepting responses, so just bail.
    return;
  }

  std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>
      arg_list;
  auto parsed_error = ConvertFileSystemInfoResponse(response, &arg_list);

  if (parsed_error) {
    LogSignalCollectionFailed(signal_name(), start_time, parsed_error->error,
                              parsed_error->is_top_level_error);
    Respond(Error(device_signals::ErrorToString(parsed_error->error)));
    return;
  }

  LogSignalCollectionSucceeded(signal_name(), start_time, arg_list.size(),
                               request_items_count);
  Respond(ArgumentList(
      api::enterprise_reporting_private::GetFileSystemInfo::Results::Create(
          arg_list)));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// getSettings

EnterpriseReportingPrivateGetSettingsFunction::
    EnterpriseReportingPrivateGetSettingsFunction() = default;
EnterpriseReportingPrivateGetSettingsFunction::
    ~EnterpriseReportingPrivateGetSettingsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetSettingsFunction::Run() {
  if (!IsNewFunctionEnabled(
          enterprise_signals::features::NewEvFunction::kSettings)) {
    return RespondNow(Error(device_signals::ErrorToString(
        device_signals::SignalCollectionError::kUnsupported)));
  }

  std::optional<api::enterprise_reporting_private::GetSettings::Params> params =
      api::enterprise_reporting_private::GetSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Verify that all paths strings are UTF8.
  bool paths_are_all_utf8 = true;
  for (const auto& api_options_param : params->request.options) {
    if (!base::IsStringUTF8(api_options_param.path)) {
      paths_are_all_utf8 = false;
      break;
    }
  }
  EXTENSION_FUNCTION_VALIDATE(paths_are_all_utf8);

  auto aggregation_request = CreateAggregationRequest(signal_name());
  aggregation_request.settings_signal_parameters =
      ConvertSettingsOptions(params->request.options);

  const size_t number_of_items =
      aggregation_request.settings_signal_parameters.size();
  LogSignalCollectionRequestedWithItems(signal_name(), number_of_items);

  StartSignalCollection(
      params->request.user_context.user_id, aggregation_request,
      browser_context(),
      base::BindOnce(
          &EnterpriseReportingPrivateGetSettingsFunction::OnSignalRetrieved,
          this, base::TimeTicks::Now(), number_of_items));

  return RespondLater();
}

void EnterpriseReportingPrivateGetSettingsFunction::OnSignalRetrieved(
    base::TimeTicks start_time,
    size_t request_items_count,
    device_signals::SignalsAggregationResponse response) {
  if (!CanReturnResponse(browser_context())) {
    // The browser is no longer accepting responses, so just bail.
    return;
  }

  std::vector<api::enterprise_reporting_private::GetSettingsResponse> arg_list;
  auto parsed_error = ConvertSettingsResponse(response, &arg_list);

  if (parsed_error) {
    LogSignalCollectionFailed(signal_name(), start_time, parsed_error->error,
                              parsed_error->is_top_level_error);
    Respond(Error(device_signals::ErrorToString(parsed_error->error)));
    return;
  }

  LogSignalCollectionSucceeded(signal_name(), start_time, arg_list.size(),
                               request_items_count);
  Respond(ArgumentList(
      api::enterprise_reporting_private::GetSettings::Results::Create(
          arg_list)));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

// getAvInfo

EnterpriseReportingPrivateGetAvInfoFunction::
    EnterpriseReportingPrivateGetAvInfoFunction() = default;
EnterpriseReportingPrivateGetAvInfoFunction::
    ~EnterpriseReportingPrivateGetAvInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetAvInfoFunction::Run() {
  if (!IsNewFunctionEnabled(
          enterprise_signals::features::NewEvFunction::kAntiVirus)) {
    return RespondNow(Error(device_signals::ErrorToString(
        device_signals::SignalCollectionError::kUnsupported)));
  }

  std::optional<api::enterprise_reporting_private::GetAvInfo::Params> params =
      api::enterprise_reporting_private::GetAvInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  StartSignalCollection(
      params->user_context.user_id, CreateAggregationRequest(signal_name()),
      browser_context(),
      base::BindOnce(
          &EnterpriseReportingPrivateGetAvInfoFunction::OnSignalRetrieved, this,
          base::TimeTicks::Now()));

  return RespondLater();
}

void EnterpriseReportingPrivateGetAvInfoFunction::OnSignalRetrieved(
    base::TimeTicks start_time,
    device_signals::SignalsAggregationResponse response) {
  if (!CanReturnResponse(browser_context())) {
    // The browser is no longer accepting responses, so just bail.
    return;
  }

  std::vector<api::enterprise_reporting_private::AntiVirusSignal> arg_list;
  auto parsed_error = ConvertAvProductsResponse(response, &arg_list);

  if (parsed_error) {
    LogSignalCollectionFailed(signal_name(), start_time, parsed_error->error,
                              parsed_error->is_top_level_error);
    Respond(Error(device_signals::ErrorToString(parsed_error->error)));
    return;
  }

  LogSignalCollectionSucceeded(signal_name(), start_time, arg_list.size());
  Respond(ArgumentList(
      api::enterprise_reporting_private::GetAvInfo::Results::Create(arg_list)));
}

// getHotfixes

EnterpriseReportingPrivateGetHotfixesFunction::
    EnterpriseReportingPrivateGetHotfixesFunction() = default;
EnterpriseReportingPrivateGetHotfixesFunction::
    ~EnterpriseReportingPrivateGetHotfixesFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetHotfixesFunction::Run() {
  if (!IsNewFunctionEnabled(
          enterprise_signals::features::NewEvFunction::kHotfix)) {
    return RespondNow(Error(device_signals::ErrorToString(
        device_signals::SignalCollectionError::kUnsupported)));
  }

  std::optional<api::enterprise_reporting_private::GetHotfixes::Params> params =
      api::enterprise_reporting_private::GetHotfixes::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  StartSignalCollection(
      params->user_context.user_id, CreateAggregationRequest(signal_name()),
      browser_context(),
      base::BindOnce(
          &EnterpriseReportingPrivateGetHotfixesFunction::OnSignalRetrieved,
          this, base::TimeTicks::Now()));

  return RespondLater();
}

void EnterpriseReportingPrivateGetHotfixesFunction::OnSignalRetrieved(
    base::TimeTicks start_time,
    device_signals::SignalsAggregationResponse response) {
  if (!CanReturnResponse(browser_context())) {
    // The browser is no longer accepting responses, so just bail.
    return;
  }

  std::vector<api::enterprise_reporting_private::HotfixSignal> arg_list;
  auto parsed_error = ConvertHotfixesResponse(response, &arg_list);

  if (parsed_error) {
    LogSignalCollectionFailed(signal_name(), start_time, parsed_error->error,
                              parsed_error->is_top_level_error);
    Respond(Error(device_signals::ErrorToString(parsed_error->error)));
    return;
  }

  LogSignalCollectionSucceeded(signal_name(), start_time, arg_list.size());
  Respond(ArgumentList(
      api::enterprise_reporting_private::GetHotfixes::Results::Create(
          arg_list)));
}

#endif  // BUILDFLAG(IS_WIN)

// reportDataMaskingEvent

EnterpriseReportingPrivateReportDataMaskingEventFunction::
    EnterpriseReportingPrivateReportDataMaskingEventFunction() = default;
EnterpriseReportingPrivateReportDataMaskingEventFunction::
    ~EnterpriseReportingPrivateReportDataMaskingEventFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateReportDataMaskingEventFunction::Run() {
  auto params =
      api::enterprise_reporting_private::ReportDataMaskingEvent::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  enterprise_connectors::ReportDataMaskingEvent(browser_context(),
                                                std::move(params->event));

  return RespondNow(NoArguments());
}

}  // namespace extensions
