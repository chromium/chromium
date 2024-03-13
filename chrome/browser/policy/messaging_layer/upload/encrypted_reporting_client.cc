// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
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
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace reporting {

namespace {

// Returns `true` if HTTP response code indicates an irrecoverable error.
bool IsIrrecoverableError(int response_code) {
  return response_code >= ::net::HTTP_BAD_REQUEST &&
         response_code < ::net::HTTP_INTERNAL_SERVER_ERROR &&
         response_code !=
             ::net::HTTP_CONFLICT;  // Overlapping seq_id ranges detected
}

// Generates new backoff entry.
std::unique_ptr<::net::BackoffEntry> GetBackoffEntry(Priority priority) {
  // Retry policy for SECURITY queue.
  static const ::net::BackoffEntry::Policy kSecurityUploadBackoffPolicy = {
      // Number of initial errors to ignore before applying
      // exponential back-off rules.
      /*num_errors_to_ignore=*/0,

      // Initial delay is 10 seconds.
      /*initial_delay_ms=*/10 * 1000,

      // Factor by which the waiting time will be multiplied.
      /*multiply_factor=*/2,

      // Fuzzing percentage.
      /*jitter_factor=*/0.1,

      // Maximum delay is 1 minute.
      /*maximum_backoff_ms=*/1 * 60 * 1000,

      // It's up to the caller to reset the backoff time.
      /*entry_lifetime_ms=*/-1,

      /*always_use_initial_delay=*/true,
  };
  // Retry policy for all other queues, including initial key delivery.
  static const ::net::BackoffEntry::Policy kDefaultUploadBackoffPolicy = {
      // Number of initial errors to ignore before applying
      // exponential back-off rules.
      /*num_errors_to_ignore=*/0,

      // Initial delay is 10 seconds.
      /*initial_delay_ms=*/10 * 1000,

      // Factor by which the waiting time will be multiplied.
      /*multiply_factor=*/2,

      // Fuzzing percentage.
      /*jitter_factor=*/0.1,

      // Maximum delay is 24 hours.
      /*maximum_backoff_ms=*/24 * 60 * 60 * 1000,

      // It's up to the caller to reset the backoff time.
      /*entry_lifetime_ms=*/-1,

      /*always_use_initial_delay=*/true,
  };
  // Maximum backoff is set per priority. Current proposal is to set SECURITY
  // events to be backed off only slightly: max delay is set to 1 minute.
  // For all other priorities max delay is set to 24 hours.
  auto backoff_entry = std::make_unique<::net::BackoffEntry>(
      priority == Priority::SECURITY ? &kSecurityUploadBackoffPolicy
                                     : &kDefaultUploadBackoffPolicy);
  return backoff_entry;
}

// State of single priority queue uploads.
// It is a singleton, protected implicitly by the fact that all relevant
// EncryptedReportingJobConfiguration actions are called on the sequenced task
// runner.
struct UploadState {
  // Keyed by priority+generation_id with explicit hash.
  using Key = std::pair<Priority, int64_t /*generation_id*/>;
  struct Hash {
    std::size_t operator()(const Key& key) const noexcept {
      const std::size_t h1 = std::hash<Priority>{}(key.first);
      const std::size_t h2 = std::hash<int64_t>{}(key.second);
      return h1 ^ (h2 << 1);  // hash_combine
    }
  };

  // Highest sequence id that has been posted for upload.
  int64_t last_sequence_id;

  // Time when the next request will be allowed.
  // This is essentially the cache value of the backoff->GetReleaseTime().
  // When the time is reached, one request is allowed, backoff is updated as if
  // the request failed, and the new release time is cached.
  base::TimeTicks earliest_retry_timestamp;

  // Current backoff entry for this priority.
  std::unique_ptr<::net::BackoffEntry> backoff_entry;
};
// Unordered map of all the queues states.
using UploadStateMap =
    std::unordered_map<UploadState::Key, UploadState, UploadState::Hash>;

UploadStateMap* state_map() {
  static base::NoDestructor<UploadStateMap> map;
  return map.get();
}

UploadState* GetState(Priority priority,
                      int64_t generation_id,
                      int64_t sequence_id) {
  auto key = std::make_pair(priority, generation_id);
  auto state_it = state_map()->find(key);
  if (state_it == state_map()->end()) {
    // This priority+generation_id pop up for the first time.
    // Record new state and allow upload.
    state_it = state_map()
                   ->emplace(std::make_pair(
                       std::move(key),
                       UploadState{.last_sequence_id = sequence_id,
                                   .backoff_entry = GetBackoffEntry(priority)}))
                   .first;
    state_it->second.earliest_retry_timestamp =
        state_it->second.backoff_entry->GetReleaseTime();
  }
  return &state_it->second;
}

// Builds uploading payload.
// Returns dictionary (null in case of failure) and matching memory
// reservation.
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
  // |ShouldReport|, both of which of all instances of this class should only
  // be called in the same sequence.
  static base::Time last_reported_time_;

  // Response payload size. Negative means not set yet.
  int response_payload_size_ = -1;
};

// static
base::Time PayloadSizeUmaReporter::last_reported_time_{base::Time::UnixEpoch()};

// Limits the rate at which payload sizes are computed for UMA reporting
// purposes. Since computing payload size is expensive, this is for limiting
// how frequently they are computed.

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

void EncryptedReportingClient::PresetUploads(base::Value::Dict context,
                                             std::string dm_token,
                                             std::string client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_ = std::move(context);
  dm_token_ = std::move(dm_token);
  client_id_ = std::move(client_id);
}

// static
std::unique_ptr<EncryptedReportingClient> EncryptedReportingClient::Create(
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(new EncryptedReportingClient(std::move(delegate)));
}

EncryptedReportingClient::EncryptedReportingClient(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

EncryptedReportingClient::~EncryptedReportingClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EncryptedReportingClient::UploadReport(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeDelta delay = WhenIsAllowedToProceed(records);
  if (delay.is_positive()) {
    // Reject upload.
    std::move(callback).Run(base::unexpected(
        Status(error::OUT_OF_RANGE, "Too many upload requests")));
    return;
  }
  // Accept upload.
  AccountForAllowedJob(records);

  // Perform upload.
  // TODO(b/327243582): Move the latter to actual upload from UploadState cache.
  PerformUpload(need_encryption_key, config_file_version, std::move(records),
                std::move(scoped_reservation), std::move(callback));
}

void EncryptedReportingClient::PerformUpload(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ResponseCallback callback) {
  // Construct payload on thread pool, then resume action on the current thread.
  // Perform Build on a thread pool, and upload result on UI.
  Priority priority = Priority::UNDEFINED_PRIORITY;
  int64_t last_generation_id = -1L;
  int64_t last_sequence_id = -1L;
  if (!records.empty()) {
    const auto& last_sequence_info = records.crbegin()->sequence_information();
    priority = last_sequence_info.priority();
    last_generation_id = last_sequence_info.generation_id();
    last_sequence_id = last_sequence_info.sequencing_id();
  }
  auto create_job_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &EncryptedReportingClient::CreateUploadJob,
      weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&EncryptedReportingClient::AccountForUploadResponse,
                     priority, last_generation_id, last_sequence_id),
      std::move(callback)));
  base::ThreadPool::PostTask(
      FROM_HERE, {},
      base::BindOnce(&BuildPayload, GenerationGuidIsRequired(),
                     need_encryption_key, config_file_version,
                     std::move(records), std::move(scoped_reservation),
                     std::move(create_job_cb)));
}

void EncryptedReportingClient::CreateUploadJob(
    policy::EncryptedReportingJobConfiguration::UploadResponseCallback
        response_cb,
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

  std::optional<int> request_payload_size;
  if (PayloadSizeComputationRateLimiterForUma::Get().ShouldDo()) {
    request_payload_size = GetPayloadSize(payload_result.value());
  }

  if (context_.empty()) {
    std::move(callback).Run(base::unexpected(
        Status(error::FAILED_PRECONDITION, "Upload context not preset")));
    return;
  }

  auto config = std::make_unique<policy::EncryptedReportingJobConfiguration>(
      g_browser_process->shared_url_loader_factory(),
      device_management_service->configuration()
          ->GetEncryptedReportingServerUrl(),
      std::move(payload_result.value()), dm_token_, client_id_,
      std::move(response_cb),
      base::BindOnce(&EncryptedReportingClient::OnReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(scoped_reservation), request_payload_size,
                     payload_size_per_hour_uma_reporter_.GetWeakPtr(),
                     std::move(callback)));

  config->UpdateContext(context_.Clone());

  auto job = device_management_service->CreateJob(std::move(config));
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
  if (response_code == ::net::HTTP_TOO_MANY_REQUESTS) {
    std::move(callback).Run(base::unexpected(
        Status(error::OUT_OF_RANGE, "Too many upload requests")));
    return;
  }
  if (response_code != ::net::HTTP_OK) {
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

  UploadResponseParser response_parser(
      EncryptedReportingClient::GenerationGuidIsRequired(),
      std::move(response.value()));
  std::move(callback).Run(std::move(response_parser));
}

// static
base::TimeDelta EncryptedReportingClient::WhenIsAllowedToProceed(
    const std::vector<EncryptedRecord>& records) {
  // If there are no records, allow upload (it will not overload the server).
  if (records.empty()) {
    return base::TimeDelta();  // 0 - allowed right away.
  }
  // Now pick up the state.
  const auto& last_sequence_info = records.crbegin()->sequence_information();
  const auto* const state = GetState(last_sequence_info.priority(),
                                     last_sequence_info.generation_id(),
                                     last_sequence_info.sequencing_id());
  // Use and update previously recorded state, base upload decision on it.
  if (state->last_sequence_id > last_sequence_info.sequencing_id()) {
    // Sequence id decreased, the upload is outdated, reject it forever.
    return base::TimeDelta::Max();
  }
  if (state->last_sequence_id < last_sequence_info.sequencing_id()) {
    // Sequence id increased, keep validating.
    switch (last_sequence_info.priority()) {
      case Priority::SECURITY:
        // For SECURITY events the request is allowed.
        return base::TimeDelta();  // 0 - allowed right away.
      default: {
        // For all other priorities we will act like in case of request’s
        // last_sequence_id is == last_sequence_id above - observing the
        // backoff time expiration.
      }
    }
  }
  // Allow upload only if earliest retry time has passed.
  // Return delta till the allowed time - if positive, upload is going to be
  // rejected.
  return state->earliest_retry_timestamp -
         state->backoff_entry->GetTimeTicksNow();
}

// static
void EncryptedReportingClient::AccountForAllowedJob(
    const std::vector<EncryptedRecord>& records) {
  Priority priority = Priority::UNDEFINED_PRIORITY;
  int64_t last_generation_id = -1L;
  int64_t last_sequence_id = -1L;
  if (!records.empty()) {
    const auto& last_sequence_info = records.crbegin()->sequence_information();
    priority = last_sequence_info.priority();
    last_generation_id = last_sequence_info.generation_id();
    last_sequence_id = last_sequence_info.sequencing_id();
  }
  auto* const state = GetState(priority, last_generation_id, last_sequence_id);
  // Update state to reflect highest sequence_id_ (we never allow upload with
  // lower sequence_id_).
  state->last_sequence_id = last_sequence_id;
  // Calculate delay as exponential backoff (based on the retry_count).
  // Update backoff under assumption that this request fails.
  // If it is responded successfully, we will reset it.
  state->backoff_entry->InformOfRequest(/*succeeded=*/false);
  state->earliest_retry_timestamp = state->backoff_entry->GetReleaseTime();
}

// static
void EncryptedReportingClient::AccountForUploadResponse(Priority priority,
                                                        int64_t generation_id,
                                                        int64_t sequence_id,
                                                        int net_error,
                                                        int response_code) {
  // Analyze the net error and update upload state for possible future retries.
  auto* const state = GetState(priority, generation_id, sequence_id);
  if (net_error != ::net::OK) {
    // Network error
  } else if (IsIrrecoverableError(response_code)) {
    // Irrecoverable error code returned by server,
    // impose artificial 24h backoff.
    state->backoff_entry->SetCustomReleaseTime(
        state->backoff_entry->GetTimeTicksNow() + base::Days(1));
  }
  // For all other cases keep the currently set retry time.
  // In case of success, inform backoff entry about that.
  if (net_error == ::net::OK && response_code == ::net::HTTP_OK) {
    state->backoff_entry->InformOfRequest(/*succeeded=*/true);
  }
  // Cache earliest retry time based on the current backoff entry.
  state->earliest_retry_timestamp = state->backoff_entry->GetReleaseTime();
}

// static
void EncryptedReportingClient::ResetUploadsStateForTest() {
  CHECK_IS_TEST();
  state_map()->clear();
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
