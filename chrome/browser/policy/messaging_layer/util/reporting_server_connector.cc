// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::policy::CloudPolicyClient;
using ::policy::CloudPolicyCore;

namespace reporting {
namespace {

// Returns `true` if device info should be included in the upload, `false`
// otherwise.
bool DeviceInfoRequiredForUpload() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return !base::FeatureList::IsEnabled(kEnableReportingFromUnmanagedDevices) ||
         // Check if this is a managed device.
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}
}  // namespace

// TODO(b/281905099): remove after rolling out reporting managed user events
// from unmanaged devices
BASE_FEATURE(kEnableReportingFromUnmanagedDevices,
             "EnableReportingFromUnmanagedDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);

ReportingServerConnector::ReportingServerConnector()
    : encrypted_reporting_client_(EncryptedReportingClient::Create()) {
  // Initialize `ReportingServerConnector` instance. For non-Ash configurations
  // it is initialized on the first use, but for Ash we need it to be prepared
  // for encryption key delivery early after enrollment.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::IgnoreResult(EnsureUsableClient());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ReportingServerConnector::~ReportingServerConnector() {
  if (client_) {
    // Notify observers.
    for (auto ob : observers_) {
      ob->OnDisconnected();
    }
    client_ = nullptr;
  }
  observers_.clear();

  if (core_) {
    core_->RemoveObserver(this);
    core_ = nullptr;
  }
}

// static
ReportingServerConnector* ReportingServerConnector::GetInstance() {
  return base::Singleton<ReportingServerConnector>::get();
}

// CloudPolicyCore::Observer implementation

// Called after the core is connected.
void ReportingServerConnector::OnCoreConnected(CloudPolicyCore* core) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  const auto client_status = EnsureUsableClient();
  LOG_IF(WARNING, !client_status.ok()) << client_status;

  // Notify observers.
  for (auto ob : observers_) {
    ob->OnConnected();
  }
}

// Called after the refresh scheduler is started (unused here).
void ReportingServerConnector::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

// Called before the core is disconnected.
void ReportingServerConnector::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);

  // Notify observers.
  for (auto ob : observers_) {
    ob->OnDisconnected();
  }

  client_ = nullptr;
}

// Called before the core is destructed.
void ReportingServerConnector::OnCoreDestruction(CloudPolicyCore* core) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  core->RemoveObserver(this);
  core_ = nullptr;
}

// static
void ReportingServerConnector::UploadEncryptedReport(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb,
    ResponseCallback callback) {
  // This function should be called on the UI task runner, and if it isn't, it
  // reschedules itself to do so.
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                       need_encryption_key, config_file_version,
                       std::move(records), std::move(scoped_reservation),
                       std::move(enqueued_cb), std::move(callback)));
    return;
  }
  // Now we are on UI task runner.
  GetInstance()->UploadEncryptedReportInternal(
      need_encryption_key, config_file_version, std::move(records),
      std::move(scoped_reservation), std::move(enqueued_cb),
      std::move(callback));
}

void ReportingServerConnector::UploadEncryptedReportInternal(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb,
    ResponseCallback callback) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);

  // Add context elements needed by reporting server.
  base::Value::Dict context;
  std::string dm_token;
  std::string client_id;
  context.Set(json_keys::kBrowser,
              base::Value::Dict().Set(json_keys::kUserAgent,
                                      embedder_support::GetUserAgent()));
  if (DeviceInfoRequiredForUpload()) {
    // Initialize the cloud policy client.
    auto client_status = EnsureUsableClient();
    if (!client_status.ok()) {
      std::move(enqueued_cb).Run(base::unexpected(client_status));
      std::move(callback).Run(base::unexpected(std::move(client_status)));
      return;
    }
    dm_token = client_->dm_token();
    client_id = client_->client_id();
    if (dm_token.empty()) {
      Status no_dm_token_status{error::UNAVAILABLE, "Device DM token not set"};
      std::move(enqueued_cb).Run(base::unexpected(no_dm_token_status));
      std::move(callback).Run(base::unexpected(std::move(no_dm_token_status)));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::DEVICE_DM_TOKEN_NOT_SET,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    context.Set(json_keys::kDevice,
                base::Value::Dict().Set(json_keys::kDmToken, dm_token));
  }

  // Add context elements needed by reporting server.
  context.Set(json_keys::kBrowser,
              base::Value::Dict().Set(json_keys::kUserAgent,
                                      embedder_support::GetUserAgent()));

  encrypted_reporting_client_->PresetUploads(
      std::move(context), std::move(dm_token), std::move(client_id));

  // Forward the `UploadEncryptedReport` to `client`.
  encrypted_reporting_client_->UploadReport(
      need_encryption_key, config_file_version, std::move(records),
      std::move(scoped_reservation), std::move(enqueued_cb),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          [](ResponseCallback callback, StatusOr<UploadResponseParser> result) {
            DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
            if (!result.has_value()) {
              std::move(callback).Run(std::move(result));
              return;
            }
            std::move(callback).Run(std::move(result.value()));
          },
          std::move(callback))));
}

StatusOr<::policy::CloudPolicyManager*>
ReportingServerConnector::GetUserCloudPolicyManager() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  // Pointer to `policy::CloudPolicyManager` is retrieved differently
  // for ChromeOS-Ash, for Android and for all other cases.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::InstallAttributes::IsInitialized()) {
    CHECK_IS_TEST();
    return base::unexpected(
        Status(error::UNAVAILABLE, "InstallAttributes not initialized"));
  }
  if (!g_browser_process || !g_browser_process->platform_part() ||
      !g_browser_process->platform_part()->browser_policy_connector_ash()) {
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CANNOT_GET_CLOUD_POLICY_MANAGER_FOR_BROWSER,
        UnavailableErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::UNAVAILABLE,
               "Browser process not fit to retrieve CloudPolicyManager"));
  }
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceCloudPolicyManager();
#elif BUILDFLAG(IS_ANDROID)
  // Android doesn't have access to a device level CloudPolicyClient, so get
  // the PrimaryUserProfile CloudPolicyClient.
  if (!ProfileManager::GetPrimaryUserProfile()) {
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CANNOT_GET_CLOUD_POLICY_MANAGER_FOR_PROFILE,
        UnavailableErrorReason::MAX_VALUE);
    return base::unexpected(Status(error::UNAVAILABLE,
                                   "PrimaryUserProfile not fit to retrieve "
                                   "CloudPolicyManager"));
  }
  return ProfileManager::GetPrimaryUserProfile()->GetUserCloudPolicyManager();
#else
  if (!g_browser_process || !g_browser_process->browser_policy_connector()) {
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CANNOT_GET_CLOUD_POLICY_MANAGER_FOR_BROWSER,
        UnavailableErrorReason::MAX_VALUE);
    return base::unexpected(Status(error::UNAVAILABLE,
                                   "Browser process not fit to retrieve "
                                   "CloudPolicyManager"));
  }
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
#endif
}

Status ReportingServerConnector::EnsureUsableCore() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  // The `policy::CloudPolicyCore` object is retrieved in two different ways
  // for ChromeOS and non-ChromeOS browsers.
  if (!core_) {
    ASSIGN_OR_RETURN(::policy::CloudPolicyManager* const cloud_policy_manager,
                     GetUserCloudPolicyManager());
    if (cloud_policy_manager == nullptr) {
      return Status(error::FAILED_PRECONDITION,
                    "This is not a managed device or browser");
    }
    if (cloud_policy_manager->core() == nullptr) {
      return Status(error::NOT_FOUND, "No usable CloudPolicyCore found");
    }
    // Cache core and keep an eye on it being alive.
    core_ = cloud_policy_manager->core();
    core_->AddObserver(this);
  }

  // Core is usable.
  return Status::StatusOK();
}

Status ReportingServerConnector::EnsureUsableClient() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  // The `policy::CloudPolicyClient` object is retrieved in two different ways
  // for ChromeOS and non-ChromeOS browsers.
  if (!client_) {
    RETURN_IF_ERROR_STATUS(EnsureUsableCore());

    if (core_->client() == nullptr) {
      return Status(error::NOT_FOUND, "No usable CloudPolicyClient found");
    }

    // Client is now available, cache it.
    client_ = core_->client();
  }

  if (!client_->is_registered()) {
    return Status(error::FAILED_PRECONDITION,
                  "CloudPolicyClient is not in registered state");
  }

  // Client is usable.
  return Status::StatusOK();
}

void ReportingServerConnector::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  CHECK(observer);
  for (auto ob : observers_) {
    CHECK_NE(ob, observer) << "Observer cannot be registered more than once";
  }
  observers_.emplace_back(observer);
}

void ReportingServerConnector::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (it->get() == observer) {
      it = observers_.erase(it);
    } else {
      ++it;
    }
  }
}
}  // namespace reporting
