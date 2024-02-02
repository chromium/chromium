// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace reporting {

namespace {
// Builds uploading payload.
// Returns dictionary (null in case of failure) and matching memory reservation.
void BuildPayload(
    bool is_generation_guid_required,
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    base::OnceCallback<void(std::optional<base::Value::Dict> /*payload_result*/,
                            ScopedReservation /*scoped_reservation*/)>
        create_job_cb) {
  // Prepare request builder.
  UploadEncryptedReportingRequestBuilder request_builder{
      is_generation_guid_required, need_encryption_key, config_file_version};
  // Hand over all records to it.
  for (auto& record : records) {
    request_builder.AddRecord(std::move(record), scoped_reservation);
  }
  // Assign random UUID as the request id for server side log correlation
  const auto request_id = base::Token::CreateRandom().ToString();
  request_builder.SetRequestId(request_id);
  // Build payload and create job.
  return std::move(create_job_cb)
      .Run(request_builder.Build(), std::move(scoped_reservation));
}

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

// Limits the rate at which payload sizes are computed for UMA reporting
// purposes. Since computing payload size is expensive, this is for limiting how
// frequently they are computed.

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

// Gets the size of payload as a JSON string.
static int GetPayloadSize(const base::Value::Dict& payload) {
  std::string payload_json;
  base::JSONWriter::Write(payload, &payload_json);
  return static_cast<int>(payload_json.size());
}
}  // namespace

policy::DeviceManagementService*
EncryptedReportingClient::Delegate::device_management_service() const {
  if (!g_browser_process || !g_browser_process->browser_policy_connector()) {
    return nullptr;
  }
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

// Returns true if a generation guid is required for this device or browser.
// Returns false otherwise.
// static
bool EncryptedReportingClient::GenerationGuidIsRequired() {
#if BUILDFLAG(IS_CHROMEOS)
  // Returns true if this is an unmanaged ChromeOS device.
  // Generation guid is only required for unmanaged ChromeOS devices. Enterprise
  // managed ChromeOS devices or device with managed browser are not required to
  // use the version of `Storage` that produces generation guids.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return !policy::ManagementServiceFactory::GetForPlatform()
              ->HasManagementAuthority(
                  policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
#else
  // For non-ChromeOS returns false.
  return false;
#endif
}

// static
std::unique_ptr<EncryptedReportingClient> EncryptedReportingClient::Create(
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(new EncryptedReportingClient(
      GenerationGuidIsRequired(), std::move(delegate)));
}

EncryptedReportingClient::EncryptedReportingClient(
    bool is_generation_guid_required,
    std::unique_ptr<Delegate> delegate)
    : is_generation_guid_required_(is_generation_guid_required),
      delegate_(std::move(delegate)) {}

EncryptedReportingClient::~EncryptedReportingClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EncryptedReportingClient::UploadReport(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    std::optional<base::Value::Dict> context,
    policy::CloudPolicyClient* cloud_policy_client,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Construct payload on thread pool, then resume action on the current thread.
  // Perform Build on a thread pool, and upload result on UI.
  auto create_job_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&EncryptedReportingClient::CreateUploadJob,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     cloud_policy_client, std::move(callback)));
  base::ThreadPool::PostTask(
      FROM_HERE, {},
      base::BindOnce(&BuildPayload, is_generation_guid_required_,
                     need_encryption_key, config_file_version,
                     std::move(records), std::move(scoped_reservation),
                     std::move(create_job_cb)));
}

void EncryptedReportingClient::CreateUploadJob(
    std::optional<base::Value::Dict> context,
    policy::CloudPolicyClient* cloud_policy_client,
    ResponseCallback callback,
    std::optional<base::Value::Dict> payload_result,
    ScopedReservation scoped_reservation) {
  if (!payload_result.has_value()) {
    std::move(callback).Run(base::unexpected(
        Status(error::FAILED_PRECONDITION, "Failure to build request")));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::DeviceManagementService* const device_management_service =
      delegate_->device_management_service();
  if (!device_management_service) {
    std::move(callback).Run(base::unexpected(
        Status(error::NOT_FOUND,
               "Device management service required, but not found")));
    return;
  }

  // This is the case for uploading managed user events from an
  // unmanaged device. The server will authenticate by looking at the user dm
  // tokens inside the records instead of a single request-level device dm
  // token.
  policy::DMAuth auth_data = policy::DMAuth::NoAuth();

  if (cloud_policy_client) {
    // The device cloud policy client only exists on managed devices and is the
    // source of the DM token. So if the device is managed, we use the device dm
    // token as authentication.
    auth_data = policy::DMAuth::FromDMToken(cloud_policy_client->dm_token());
  }

  std::optional<int> request_payload_size;
  if (PayloadSizeComputationRateLimiterForUma::Get().ShouldDo()) {
    request_payload_size = GetPayloadSize(payload_result.value());
  }

  auto config = std::make_unique<policy::EncryptedReportingJobConfiguration>(
      g_browser_process->shared_url_loader_factory(), std::move(auth_data),
      device_management_service->configuration()
          ->GetEncryptedReportingServerUrl(),
      std::move(payload_result.value()), cloud_policy_client,
      base::BindOnce(&EncryptedReportingClient::OnReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(scoped_reservation), request_payload_size,
                     payload_size_per_hour_uma_reporter_.GetWeakPtr(),
                     std::move(callback)));

  if (context.has_value()) {
    config->UpdateContext(std::move(context.value()));
  }
  const base::TimeDelta delay = config->WhenIsAllowedToProceed();
  if (delay.is_positive()) {
    // Reject upload.
    config->CancelNotAllowedJob();  // Invokes callback to response back.
    return;
  }
  // Accept upload.
  config->AccountForAllowedJob();
  std::unique_ptr<policy::DeviceManagementService::Job> job =
      device_management_service->CreateJob(std::move(config));
  request_jobs_.emplace(std::move(job));
}

void EncryptedReportingClient::OnReportUploadCompleted(
    ScopedReservation scoped_reservation,
    std::optional<int> request_payload_size,
    base::WeakPtr<PayloadSizePerHourUmaReporter>
        payload_size_per_hour_uma_reporter,
    ResponseCallback callback,
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus status,
    int response_code,
    std::optional<base::Value::Dict> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (job) {
    request_jobs_.erase(job);
  }
  if (response_code == ::policy::DeviceManagementService::kTooManyRequests) {
    std::move(callback).Run(base::unexpected(
        Status(error::OUT_OF_RANGE, "Too many upload requests")));
    return;
  }
  if (response_code != ::policy::DeviceManagementService::kSuccess) {
    std::move(callback).Run(base::unexpected(
        Status(error::DATA_LOSS,
               base::StrCat(
                   {"Response code: ", base::NumberToString(response_code)}))));
    return;
  }
  if (!response.has_value()) {
    std::move(callback).Run(base::unexpected(
        Status(error::DATA_LOSS, "Success response is empty")));
    return;
  }

  PayloadSizeComputationRateLimiterForUma::Get().Next();

  // If request_payload_size has value, it means the rate limiter
  // wants payload size to be computed here.
  if (request_payload_size.has_value()) {
    // Request payload has already been computed at the time of
    // request.
    const int response_payload_size = GetPayloadSize(response.value());

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

  std::move(callback).Run(std::move(response.value()));
}

// ======== PayloadSizePerHourUmaReporter ==========

// static
int EncryptedReportingClient::PayloadSizePerHourUmaReporter::ConvertBytesToKiB(
    int bytes) {
  return bytes / 1024;
}

EncryptedReportingClient::PayloadSizePerHourUmaReporter::
    PayloadSizePerHourUmaReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Start(FROM_HERE, kReportingInterval, this,
               &PayloadSizePerHourUmaReporter::Report);
}

EncryptedReportingClient::PayloadSizePerHourUmaReporter::
    ~PayloadSizePerHourUmaReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EncryptedReportingClient::PayloadSizePerHourUmaReporter::
    RecordRequestPayloadSize(int payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_payload_size_ += payload_size;
}

void EncryptedReportingClient::PayloadSizePerHourUmaReporter::
    RecordResponsePayloadSize(int payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_payload_size_ += payload_size;
}

void EncryptedReportingClient::PayloadSizePerHourUmaReporter::Report() {
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

base::WeakPtr<EncryptedReportingClient::PayloadSizePerHourUmaReporter>
EncryptedReportingClient::PayloadSizePerHourUmaReporter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
}  // namespace reporting
