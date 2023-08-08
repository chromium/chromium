// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/management_utils.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
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
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#endif

using ::policy::CloudPolicyClient;
using ::policy::CloudPolicyCore;

namespace reporting {

BASE_FEATURE(kEnableEncryptedReportingClientForUpload,
             "EnableEncryptedReportingClientForUpload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(b/281905099): remove after rolling out reporting managed user events
// from unmanaged devices
BASE_FEATURE(kEnableReportingFromUnmanagedDevices,
             "EnableReportingFromUnmanagedDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gets the size of payload as a JSON string.
static int GetPayloadSize(const base::Value::Dict& payload) {
  std::string payload_json;
  base::JSONWriter::Write(payload, &payload_json);
  return static_cast<int>(payload_json.size());
}

// Limits the rate at which payload sizes are computed for UMA reporting
// purposes. Since computing payload size is expensive, this is for limiting how
// frequently they are computed.

// (TODO: b/259747862) This class should be removed in the long run.
class PayloadSizeComputationRateLimiterForUma {
 public:
  // We compute once for every |kScaleFactor| times that upload succeeds.
  static constexpr uint64_t kScaleFactor = 10u;

  PayloadSizeComputationRateLimiterForUma() = default;
  PayloadSizeComputationRateLimiterForUma(
      const PayloadSizeComputationRateLimiterForUma&) = delete;
  PayloadSizeComputationRateLimiterForUma& operator=(
      const PayloadSizeComputationRateLimiterForUma&) = delete;

  // Gets the static instance of `PayloadSizeComputationRateLimiterForUma`.
  static PayloadSizeComputationRateLimiterForUma& Get() {
    // OK to run the destructor (No need for `NoDestructor`) -- it's trivially
    // destructible.
    static PayloadSizeComputationRateLimiterForUma rate_limiter;
    return rate_limiter;
  }

  // Should payload size be computed and recorded?
  [[nodiscard]] bool ShouldDo() const {
    DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
    return successful_upload_counter_ % kScaleFactor == 0u;
  }

  // Bumps the upload counter. Must call this once after having called
  // |ShouldDo| every time an upload succeeds.
  void Next() {
    DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
    ++successful_upload_counter_;
  }

 private:
  // A counter increases by 1 each time an upload succeeds. Starting from a
  // random number between 0 and kScaleFactor - 1, not zero.
  uint64_t successful_upload_counter_ = base::RandGenerator(kScaleFactor);
};

// Manages reporting payload sizes of single uploads via UMA.
class PayloadSizeUmaReporter {
 public:
  PayloadSizeUmaReporter() = default;
  PayloadSizeUmaReporter(const PayloadSizeUmaReporter&) = delete;
  PayloadSizeUmaReporter& operator=(const PayloadSizeUmaReporter&) = delete;
  PayloadSizeUmaReporter(PayloadSizeUmaReporter&&) = default;
  PayloadSizeUmaReporter& operator=(PayloadSizeUmaReporter&&) = default;

  // Whether payload size should be reported now.
  static bool ShouldReport() {
    DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
    return base::Time::Now() >= last_reported_time_ + kMinReportTimeDelta;
  }

  // Reports to UMA.
  void Report() {
    DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
    CHECK_GE(response_payload_size_, 0);

    last_reported_time_ = base::Time::Now();
    base::UmaHistogramCounts1M("Browser.ERP.ResponsePayloadSize",
                               response_payload_size_);
  }

  // Updates response payload size.
  void UpdateResponsePayloadSize(int response_payload_size) {
    DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
    response_payload_size_ = response_payload_size;
  }

 private:
  // Minimum amount of time between two reports.
  static constexpr base::TimeDelta kMinReportTimeDelta = base::Hours(1);

  // Last time UMA report was done. This is accessed from |Report| and
  // |ShouldReport|, both of which of all instances of this class should only be
  // called in the same sequence.
  static base::Time last_reported_time_;

  // Response payload size. Negative means not set yet.
  int response_payload_size_ = -1;
};

// static
base::Time PayloadSizeUmaReporter::last_reported_time_{base::Time::UnixEpoch()};

ReportingServerConnector::ReportingServerConnector()
    : encrypted_reporting_client_(
          std::make_unique<EncryptedReportingClient>()) {}

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
// Returns true if device info should be including in the upload. Returns false
// otherwise.
bool DeviceInfoRequiredForUpload() {
  return !base::FeatureList::IsEnabled(kEnableReportingFromUnmanagedDevices) ||
         // Check if this is a managed device.
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}

void ReportingServerConnector::UploadEncryptedReportInternal(
    base::Value::Dict merging_payload,
    absl::optional<base::Value::Dict> context,
    ResponseCallback callback) {
  if (base::FeatureList::IsEnabled(kEnableEncryptedReportingClientForUpload)) {
    encrypted_reporting_client_->UploadReport(std::move(merging_payload),
                                              std::move(context), client_,
                                              std::move(callback));
    return;
  }
  // Deprecated: uses cloud policy client.
  auto cb = base::BindOnce(
      [](ResponseCallback callback,
         absl::optional<base::Value::Dict> client_result) {
        if (!client_result.has_value()) {
          std::move(callback).Run(Status(error::DATA_LOSS, "Failed to upload"));
          return;
        }
        std::move(callback).Run(std::move(client_result.value()));
      },
      std::move(callback));
  client_->UploadEncryptedReport(std::move(merging_payload), std::move(context),
                                 std::move(cb));
}

// static
void ReportingServerConnector::UploadEncryptedReport(
    base::Value::Dict merging_payload,
    ResponseCallback callback) {
  // This function should be called on the UI task runner, and if it isn't, it
  // reschedules itself to do so.
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                       std::move(merging_payload), std::move(callback)));
    return;
  }

  // Now we are on UI task runner.
  ReportingServerConnector* const connector = GetInstance();

  // Add context elements needed by reporting server.
  base::Value::Dict context;
  context.SetByDottedPath("browser.userAgent",
                          embedder_support::GetUserAgent());

  if (DeviceInfoRequiredForUpload()) {
    // Initialize the cloud policy client
    auto client_status = connector->EnsureUsableClient();
    if (!client_status.ok()) {
      std::move(callback).Run(client_status);
      return;
    }
    if (connector->client_->dm_token().empty()) {
      std::move(callback).Run(
          Status(error::UNAVAILABLE, "Device DM token not set"));
      return;
    }
    context.SetByDottedPath("device.dmToken", connector->client_->dm_token());
  }

  // Forward the `UploadEncryptedReport` to the cloud policy client.
  absl::optional<int> request_payload_size;
  if (PayloadSizeComputationRateLimiterForUma::Get().ShouldDo()) {
    request_payload_size = GetPayloadSize(merging_payload);
  }
  connector->UploadEncryptedReportInternal(
      std::move(merging_payload), std::move(context),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          [](ResponseCallback callback,
             absl::optional<int> request_payload_size,
             base::WeakPtr<PayloadSizePerHourUmaReporter>
                 payload_size_per_hour_uma_reporter,
             StatusOr<base::Value::Dict> result) {
            DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
            if (!result.ok()) {
              std::move(callback).Run(std::move(result));
              return;
            }

            PayloadSizeComputationRateLimiterForUma::Get().Next();

            // If request_payload_size has value, it means the rate limiter
            // wants payload size to be computed here.
            if (request_payload_size.has_value()) {
              // Request payload has already been computed at the time of
              // request.
              const int response_payload_size =
                  GetPayloadSize(result.ValueOrDie());

              // Let UMA report the request and response payload sizes.
              if (PayloadSizeUmaReporter::ShouldReport()) {
                PayloadSizeUmaReporter payload_size_uma_reporter;
                payload_size_uma_reporter.UpdateResponsePayloadSize(
                    response_payload_size);
                payload_size_uma_reporter.Report();
              }

              if (payload_size_per_hour_uma_reporter) {
                payload_size_per_hour_uma_reporter->RecordRequestPayloadSize(
                    request_payload_size.value());
                payload_size_per_hour_uma_reporter->RecordResponsePayloadSize(
                    response_payload_size);
              }
            }

            std::move(callback).Run(std::move(result.ValueOrDie()));
          },
          std::move(callback), std::move(request_payload_size),
          connector->payload_size_per_hour_uma_reporter_.GetWeakPtr())));
}

StatusOr<::policy::CloudPolicyManager*>
ReportingServerConnector::GetUserCloudPolicyManager() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  // Pointer to `policy::CloudPolicyManager` is retrieved differently
  // for ChromeOS-Ash, for Android and for all other cases.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!g_browser_process || !g_browser_process->platform_part() ||
      !g_browser_process->platform_part()->browser_policy_connector_ash()) {
    return Status(error::UNAVAILABLE,
                  "Browser process not fit to retrieve CloudPolicyManager");
  }
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceCloudPolicyManager();
#elif BUILDFLAG(IS_ANDROID)
  // Android doesn't have access to a device level CloudPolicyClient, so get
  // the PrimaryUserProfile CloudPolicyClient.
  if (!ProfileManager::GetPrimaryUserProfile()) {
    return Status(error::UNAVAILABLE,
                  "PrimaryUserProfile not fit to retrieve CloudPolicyManager");
  }
  return ProfileManager::GetPrimaryUserProfile()->GetUserCloudPolicyManager();
#else
  if (!g_browser_process || !g_browser_process->browser_policy_connector()) {
    return Status(error::UNAVAILABLE,
                  "Browser process not fit to retrieve CloudPolicyManager");
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

// ======== PayloadSizePerHourUmaReporter ==========

// static
int ReportingServerConnector::PayloadSizePerHourUmaReporter::ConvertBytesToKiB(
    int bytes) {
  return bytes / 1024;
}

ReportingServerConnector::PayloadSizePerHourUmaReporter::
    PayloadSizePerHourUmaReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Start(FROM_HERE, kReportingInterval, this,
               &PayloadSizePerHourUmaReporter::Report);
}

ReportingServerConnector::PayloadSizePerHourUmaReporter::
    ~PayloadSizePerHourUmaReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReportingServerConnector::PayloadSizePerHourUmaReporter::
    RecordRequestPayloadSize(int payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_payload_size_ += payload_size;
}

void ReportingServerConnector::PayloadSizePerHourUmaReporter::
    RecordResponsePayloadSize(int payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_payload_size_ += payload_size;
}

base::WeakPtr<ReportingServerConnector::PayloadSizePerHourUmaReporter>
ReportingServerConnector::PayloadSizePerHourUmaReporter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ReportingServerConnector::PayloadSizePerHourUmaReporter::Report() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramCounts1M(
      "Browser.ERP.RequestPayloadSizePerHour",
      ConvertBytesToKiB(request_payload_size_) *
          PayloadSizeComputationRateLimiterForUma::kScaleFactor);
  base::UmaHistogramCounts1M(
      "Browser.ERP.ResponsePayloadSizePerHour",
      ConvertBytesToKiB(response_payload_size_) *
          PayloadSizeComputationRateLimiterForUma::kScaleFactor);
  request_payload_size_ = 0;
  response_payload_size_ = 0;
}
}  // namespace reporting
