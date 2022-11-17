// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "reporting_server_connector.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif

using ::policy::CloudPolicyClient;
using ::policy::CloudPolicyCore;

namespace reporting {

// Manages reporting payload size via UMA.
class PayloadSizeUmaReporter {
 public:
  PayloadSizeUmaReporter() = default;
  PayloadSizeUmaReporter(const PayloadSizeUmaReporter&) = delete;
  PayloadSizeUmaReporter& operator=(const PayloadSizeUmaReporter&) = delete;
  PayloadSizeUmaReporter(PayloadSizeUmaReporter&&) = default;
  PayloadSizeUmaReporter& operator=(PayloadSizeUmaReporter&&) = default;

  // Whether payload size should be reported now.
  bool ShouldReport() const {
    return base::Time::Now() >= last_reported_time_ + min_report_timedelta_;
  }

  // Report to UMA.
  void Report() {
    DCHECK_GE(request_payload_size_, 0);
    DCHECK_GE(response_payload_size_, 0);

    last_reported_time_ = base::Time::Now();
    base::UmaHistogramCounts1M("Browser.ERP.RequestPayloadSize",
                               request_payload_size_);
    base::UmaHistogramCounts1M("Browser.ERP.ResponsePayloadSize",
                               response_payload_size_);
  }

  // Update request payload size.
  void UpdateRequestPayloadSize(const base::Value::Dict& request_payload) {
    request_payload_size_ = GetPayloadSize(request_payload);
  }

  // Update response payload size.
  void UpdateResponsePayloadSize(const base::Value::Dict& response_payload) {
    response_payload_size_ = GetPayloadSize(response_payload);
  }

 private:
  // Gets the size of payload as a JSON string.
  static int GetPayloadSize(const base::Value::Dict& payload) {
    std::string payload_json;
    base::JSONWriter::Write(payload, &payload_json);
    return static_cast<int>(payload_json.size());
  }

  // Minimum amount of time between two reports.
  static constexpr base::TimeDelta min_report_timedelta_ = base::Hours(1);

  // Last time UMA report was done. This is accessed from |Report| and
  // |ShouldReport|, both of which of all instances of this class should only be
  // called in the same sequence.
  static base::Time last_reported_time_;

  // Request payload size. Negative means not set yet.
  int request_payload_size_ = -1;

  // Response payload size. Negative means not set yet.
  int response_payload_size_ = -1;
};

// static
base::Time PayloadSizeUmaReporter::last_reported_time_{base::Time::UnixEpoch()};

ReportingServerConnector::ReportingServerConnector() = default;

ReportingServerConnector::~ReportingServerConnector() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  if (core_) {
    core_->RemoveObserver(this);
    core_ = nullptr;
    client_ = nullptr;
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
  client_ = core->client();
}

// Called after the refresh scheduler is started (unused here).
void ReportingServerConnector::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

// Called before the core is disconnected.
void ReportingServerConnector::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
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
    base::Value::Dict merging_payload,
    absl::optional<base::Value::Dict> context,
    ResponseCallback callback) {
  // This function should be called on the UI task runner, and if it isn't, it
  // reschedules itself to do so.
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                       std::move(merging_payload), std::move(context),
                       std::move(callback)));
    return;
  }
  // Now we are on UI task runner.
  // The `policy::CloudPolicyClient` object is retrieved in two different ways
  // for ChromeOS and non-ChromeOS browsers.
  ReportingServerConnector* const connector = GetInstance();
  auto client_status = connector->EnsureUsableClient();
  if (!client_status.ok()) {
    std::move(callback).Run(client_status);
    return;
  }

  // UMA for payload size
  PayloadSizeUmaReporter payload_size_uma_reporter;
  payload_size_uma_reporter.UpdateRequestPayloadSize(merging_payload);

  // Forward the `UploadEncryptedReport` to the cloud policy client.
  connector->client_->UploadEncryptedReport(
      std::move(merging_payload), std::move(context),
      base::BindOnce(
          [](ResponseCallback callback,
             PayloadSizeUmaReporter payload_size_uma_reporter,
             absl::optional<base::Value::Dict> result) {
            if (!result.has_value()) {
              std::move(callback).Run(
                  Status(error::DATA_LOSS, "Failed to upload"));
              return;
            }

            // Let UMA report the request and response payload sizes.
            if (payload_size_uma_reporter.ShouldReport()) {
              // Request payload has already been computed at the time of
              // request.
              payload_size_uma_reporter.UpdateResponsePayloadSize(
                  result.value());
              payload_size_uma_reporter.Report();
            }

            std::move(callback).Run(std::move(result.value()));
          },
          std::move(callback), std::move(payload_size_uma_reporter)));
}

Status ReportingServerConnector::EnsureUsableCore() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  // The `policy::CloudPolicyCore` object is retrieved in two different ways
  // for ChromeOS and non-ChromeOS browsers.
  if (!core_) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!g_browser_process || !g_browser_process->platform_part() ||
        !g_browser_process->platform_part()->browser_policy_connector_ash()) {
      return Status(error::UNAVAILABLE,
                    "Browser process not fit to retrieve CloudPolicyManager");
    }
    ::policy::CloudPolicyManager* const cloud_policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceCloudPolicyManager();
#elif BUILDFLAG(IS_ANDROID)
    // Android doesn't have access to a device level CloudPolicyClient, so get
    // the PrimaryUserProfile CloudPolicyClient.
    if (!ProfileManager::GetPrimaryUserProfile()) {
      return Status(
          error::UNAVAILABLE,
          "PrimaryUserProfile not fit to retrieve CloudPolicyManager");
    }
    ::policy::CloudPolicyManager* const cloud_policy_manager =
        ProfileManager::GetPrimaryUserProfile()->GetUserCloudPolicyManager();
#else
    if (!g_browser_process || !g_browser_process->browser_policy_connector()) {
      return Status(error::UNAVAILABLE,
                    "Browser process not fit to retrieve CloudPolicyManager");
    }
    ::policy::CloudPolicyManager* const cloud_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
#endif
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
    RETURN_IF_ERROR(EnsureUsableCore());

    if (core_->client() == nullptr) {
      return Status(error::NOT_FOUND, "No usable CloudPolicyClient found");
    }

    // Core is now available, cache client.
    client_ = core_->client();
  }
  if (!client_->is_registered()) {
    return Status(error::FAILED_PRECONDITION,
                  "CloudPolicyClient is not in registered state");
  }

  // Client is usable.
  return Status::StatusOK();
}
}  // namespace reporting
