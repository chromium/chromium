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
#include "base/containers/flat_map.h"
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
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace reporting {

namespace {

// UMA that reflects whether events were processed by the server (true/false).
constexpr char kUmaRecordProcessedByServer[] =
    "Browser.ERP.RecordProcessedByServer";

// UMA that reflects events upload count: samples number of times a single event
// is sent to the server. Per-event count is incremented every time an event is
// sent, and the metrics sample is recorded once the event is confirmed by the
// server (and thus won't be sent anymore). Expected to be 1 for the majority of
// the events, although minor duplication is allowed. This counter is inexact,
// since it may be reset in rare cases uploader memory usage reaches its limit -
// tracked by Browser.ERP.UploadMemoryUsagePercent metrics.
constexpr char kEventsUploadCount[] = "Browser.ERP.EventsUploadCountExp";

// UMA that reflects cached events count: samples number of times a single event
// is received and placed in cache. Per-event count is incremented every time an
// event is added/replaced in the cache, and the metrics sample is recorded once
// the event is confirmed by the server (and thus won't be accepted for upload
// anymore). Expected to be 1 for the majority of the events, although small
// number of re-uploads is allowed. This counter is inexact, since it may be
// reset in rare cases uploader memory usage reaches its limit - tracked by
// Browser.ERP.UploadMemoryUsagePercent metrics.
constexpr char kCachedEventsCount[] = "Browser.ERP.CachedEventsCountExp";

// Returns `true` if HTTP response code indicates an irrecoverable error.
bool IsIrrecoverableError(int response_code) {
  return response_code >= ::net::HTTP_BAD_REQUEST &&
         response_code < ::net::HTTP_INTERNAL_SERVER_ERROR &&
         response_code !=
             ::net::HTTP_CONFLICT;  // Overlapping seq_id ranges detected
}

// Generates new backoff entry.
std::unique_ptr<::net::BackoffEntry> GetBackoffEntry(Priority priority) {
  // Retry policy for queues that require immediate backoff.
  static const ::net::BackoffEntry::Policy kImmediateUploadBackoffPolicy = {
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
  // and IMMEDIATE events to be backed off only slightly: max delay is set
  // to 1 minute. For all other priorities max delay is set to 24 hours.
  auto backoff_entry = std::make_unique<::net::BackoffEntry>(
      (priority == Priority::SECURITY || priority == Priority::IMMEDIATE)
          ? &kImmediateUploadBackoffPolicy
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

  // Time when the next request will be allowed.
  // This is essentially the cache value of the backoff->GetReleaseTime().
  // When the time is reached, one request is allowed, backoff is updated as if
  // the request failed, and the new release time is cached.
  base::TimeTicks earliest_retry_timestamp;

  // Current backoff entry for this priority.
  std::unique_ptr<::net::BackoffEntry> backoff_entry;

  // Cached records received from Storage (without those confirmed by the
  // server), ordered by `sequence_id`, `priority` and `generation_id` are
  // identical and match the `UploadState` key. Logically the events form a
  // queue, but may need to be inserted in the middle, so we use a `flat_map`
  // which keeps records sorted by `sequence_id`.
  base::flat_map<int64_t /*sequence_id*/, EncryptedRecord> cached_records;

  // Total memory reservation for all cached records.
  ScopedReservation scoped_reservation;

  // Upload counters per sequence id. Incremented every time an event is sent to
  // server, sampled in UMA and removed from map once the event is confirmed or
  // if the state is reset.
  // UMA is expected to see counter of 1 for the majority of events.
  base::flat_map<int64_t /*sequence_id*/, size_t> upload_counters;

  // Cached events counters per sequence id. Incremented every time an event is
  // received for upload and added to cache; sampled in UMA and removed from map
  // once the event is confirmed or if the state is reset.
  // UMA is expected to see counter of 1 for the majority of events.
  base::flat_map<int64_t /*sequence_id*/, size_t> cached_counters;

  // Highest sequence id that has been successfully sent to server
  // (but not confirmed, so it remains in `cached_records`). Events until
  // `last_sequence_id` (inclusive) are not sent to the server.
  // `last_sequence_id` is reset upon upload job completion (if successful,
  // to the last confirmed event, otherwise to -1.
  int64_t last_sequence_id = -1;

  // Current upload job in flight (nullptr is none).
  std::unique_ptr<policy::DeviceManagementService::Job> job;

  // Deadline timer of the currently running job (if any).
  // When the timer fires, the job is cancelled.
  std::unique_ptr<base::OneShotTimer> job_timer;
};
// Unordered map of all the queues states.
using UploadStateMap =
    std::unordered_map<UploadState::Key, UploadState, UploadState::Hash>;

UploadStateMap* state_map() {
  static base::NoDestructor<UploadStateMap> map;
  return map.get();
}

UploadState* GetState(Priority priority, int64_t generation_id) {
  auto key = std::make_pair(priority, generation_id);
  auto state_it = state_map()->find(key);
  if (state_it == state_map()->end()) {
    // This priority+generation_id pop up for the first time.
    // Record new state and allow upload.
    state_it = state_map()
                   ->emplace(std::make_pair(
                       std::move(key),
                       UploadState{.backoff_entry = GetBackoffEntry(priority)}))
                   .first;
    state_it->second.earliest_retry_timestamp =
        state_it->second.backoff_entry->GetReleaseTime();
  }
  return &state_it->second;
}

// Removes confirmed events from cache.
void RemoveConfirmedEventsFromCache(UploadState* state) {
  // Remove no longer needed events from cache.
  while (!state->cached_records.empty() &&
         state->cached_records.begin()->first <= state->last_sequence_id) {
    // Sample upload counter.
    if (const auto it =
            state->upload_counters.find(state->cached_records.begin()->first);
        it != state->upload_counters.end()) {
      const auto event_upload_count = it->second;
      base::UmaHistogramCounts1M(kEventsUploadCount,
                                 /*sample=*/event_upload_count);
      state->upload_counters.erase(it);
    }
    // Sample incoming counter.
    if (const auto it =
            state->cached_counters.find(state->cached_records.begin()->first);
        it != state->cached_counters.end()) {
      const auto event_cached_count = it->second;
      base::UmaHistogramCounts1M(kCachedEventsCount,
                                 /*sample=*/event_cached_count);
      state->cached_counters.erase(it);
    }
    // Remove record from cache.
    state->cached_records.erase(state->cached_records.begin());
  }
  // Reduce reserved memory.
  uint64_t records_memory = 0u;
  for (const auto& [_, record] : state->cached_records) {
    records_memory += record.ByteSizeLong();
  }
  state->scoped_reservation.Reduce(records_memory);
}

// Posts upload records count UMA.
void LogNumRecordsInUpload(uint64_t num_records) {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (policy::ManagementServiceFactory::GetForPlatform()
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    // This is a managed device, so log the upload as such.
    base::UmaHistogramCounts1000(
        "Browser.ERP.RecordsPerUploadFromManagedDevice", num_records);
  } else {
    base::UmaHistogramCounts1000(
        "Browser.ERP.RecordsPerUploadFromUnmanagedDevice", num_records);
  }
#else
  base::UmaHistogramCounts1000(
      "Browser.ERP.RecordsPerUploadFromNonChromeosDevice", num_records);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Builds uploading payload.
// Returns dictionary (null in case of failure), matching memory reservation
// and last seq id included in request.
void BuildPayload(
    bool is_generation_guid_required,
    bool need_encryption_key,
    int config_file_version,
    int64_t last_sequence_id,
    const base::flat_map<int64_t, EncryptedRecord>& records,
    ScopedReservation scoped_reservation,
    base::OnceCallback<void(std::optional<base::Value::Dict> /*payload_result*/,
                            ScopedReservation /*scoped_reservation*/,
                            int64_t /*last_sequence_id*/,
                            uint64_t /*events_to_send*/)> create_job_cb) {
  // Prepare request builder.
  UploadEncryptedReportingRequestBuilder request_builder{
      is_generation_guid_required, need_encryption_key, config_file_version};
  // Copy records to it, as long as memory reservation allows.
  uint64_t events_to_send = 0u;
  for (const auto& [seq_id, record] : records) {
    // Skip records that already have been sent to server.
    if (seq_id <= last_sequence_id) {
      continue;
    }
    // Stop if seq ids are not sequential.
    if (last_sequence_id >= 0 && seq_id != last_sequence_id + 1) {
      break;
    }
    // Reserve memory for a copy of the record.
    ScopedReservation record_reservation(record.ByteSizeLong(),
                                         scoped_reservation);
    if (!record_reservation.reserved()) {
      break;  // Out of memory.
    }
    // Bump up last seq id.
    last_sequence_id = seq_id;
    // Make a copy of the record and hand it over to the builder.
    request_builder.AddRecord(EncryptedRecord(record), record_reservation);
    scoped_reservation.HandOver(record_reservation);
    ++events_to_send;
  }
  // Assign random UUID as the request id for server side log correlation
  const auto request_id = base::Token::CreateRandom().ToString();
  request_builder.SetRequestId(request_id);
  // Log size of non-empty upload. Key-request uploads have no records.
  if (events_to_send > 0u) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&LogNumRecordsInUpload, events_to_send));
  }
  // Build payload and create job.
  std::move(create_job_cb)
      .Run(request_builder.Build(), std::move(scoped_reservation),
           last_sequence_id, events_to_send);
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
    UploadEnqueuedCallback enqueued_cb,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Priority priority = Priority::UNDEFINED_PRIORITY;
  int64_t generation_id = -1L;
  if (!records.empty()) {
    const auto& last_sequence_info = records.front().sequence_information();
    priority = last_sequence_info.priority();
    generation_id = last_sequence_info.generation_id();
  }
  auto* const state = GetState(priority, generation_id);
  // Update the cache with `records` - add new records that are absent, skip
  // duplicates.
  size_t total_added_memory = 0uL;
  for (auto& record : records) {
    if (record.sequence_information().priority() != priority ||
        record.sequence_information().generation_id() != generation_id) {
      LOG(WARNING) << "Sequencing mismatch! Record skipped.";
      record.Clear();
      continue;
    }
    const int64_t seq_id = record.sequence_information().sequencing_id();
    if (seq_id <= state->last_sequence_id) {
      // Record has already been uploaded.
      record.Clear();
      continue;
    }
    // Insert new record or replace the one cached before, either replacing the
    // event with identical one, or with a gap record (in rare cases when the
    // record triggered a permanent error by server). Since the gap replacement
    // is rare, we do not account for the possible memory decrease.
    const auto [it, success] =
        state->cached_records.insert_or_assign(seq_id, std::move(record));
    // Set or increment cached counter of the event.
    {
      const auto [counter_it, counter_inserted] =
          state->cached_counters.try_emplace(seq_id, 1u);
      if (!counter_inserted) {
        ++(counter_it->second);
      }
    }
    if (!success) {
      // `record` is already in cache, skip it.
      continue;
    }
    // `record` is new, move it to cache.
    total_added_memory += it->second.ByteSizeLong();
  }

  // Reset memory usage to newly added records only.
  scoped_reservation.Reduce(total_added_memory);
  if (scoped_reservation.reserved()) {
    // Something has been added to cache.
    state->scoped_reservation.HandOver(scoped_reservation);
  }

  // Notify about cache state.
  std::list<int64_t> cached_records_seq_ids;
  for (const auto& [seq_id, _] : state->cached_records) {
    cached_records_seq_ids.push_back(seq_id);
  }
  std::move(enqueued_cb).Run(std::move(cached_records_seq_ids));

  // Determine whether we can upload or need a delay, based on the cached state.
  const base::TimeDelta delay = WhenIsAllowedToProceed(priority, generation_id);
  if (delay.is_positive()) {
    // Reject upload.
    std::move(callback).Run(base::unexpected(
        Status(error::OUT_OF_RANGE, "Too many upload requests")));
    return;
  }

  // Perform upload, if none is running.
  MaybePerformUpload(need_encryption_key, config_file_version, priority,
                     generation_id, std::move(callback));
}

void EncryptedReportingClient::MaybePerformUpload(bool need_encryption_key,
                                                  int config_file_version,
                                                  Priority priority,
                                                  int64_t generation_id,
                                                  ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* const state = GetState(priority, generation_id);
  if (state->job) {
    // Job already in flight, do nothing.
    std::move(callback).Run(base::unexpected(
        Status(error::ALREADY_EXISTS, "Job already in flight")));
    return;
  }

  // Construct payload on thread pool, then resume action on the current thread.
  // Perform Build on a thread pool, and upload result on UI.
  auto create_job_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &EncryptedReportingClient::CreateUploadJob,
      weak_ptr_factory_.GetWeakPtr(), priority, generation_id,
      base::BindOnce(&EncryptedReportingClient::AccountForUploadResponse,
                     priority, generation_id),
      Scoped<StatusOr<UploadResponseParser>>(
          std::move(callback),
          base::unexpected(
              Status(error::UNAVAILABLE, "Client has been destructed")))));
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&BuildPayload, GenerationGuidIsRequired(),
                     need_encryption_key, config_file_version,
                     state->last_sequence_id, state->cached_records,
                     ScopedReservation(0uL, state->scoped_reservation),
                     std::move(create_job_cb)));
}

void EncryptedReportingClient::CreateUploadJob(
    Priority priority,
    int64_t generation_id,
    policy::EncryptedReportingJobConfiguration::UploadResponseCallback
        response_cb,
    ResponseCallback callback,
    std::optional<base::Value::Dict> payload_result,
    ScopedReservation scoped_reservation,
    int64_t last_sequence_id,
    uint64_t events_to_send) {
  if (!payload_result.has_value()) {
    std::move(callback).Run(base::unexpected(
        Status(error::FAILED_PRECONDITION, "Failure to build request")));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Accept upload.
  AccountForAllowedJob(priority, generation_id, last_sequence_id);

  if (!delegate_->device_management_service()) {
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
      delegate_->device_management_service()
          ->configuration()
          ->GetEncryptedReportingServerUrl(),
      std::move(payload_result.value()), dm_token_, client_id_,
      std::move(response_cb),
      base::BindOnce(
          &EncryptedReportingClient::OnReportUploadCompleted,
          weak_ptr_factory_.GetWeakPtr(), priority, generation_id,
          std::move(scoped_reservation), request_payload_size,
          payload_size_per_hour_uma_reporter_.GetWeakPtr(),
          Scoped<StatusOr<UploadResponseParser>>(
              std::move(callback),
              base::unexpected(
                  Status(error::UNAVAILABLE, "Client has been destructed")))));

  config->UpdateContext(context_.Clone());

  // Create and track the new upload job.
  auto* const state = GetState(priority, generation_id);
  state->job =
      delegate_->device_management_service()->CreateJob(std::move(config));
  state->job_timer = std::make_unique<base::OneShotTimer>();
  state->job_timer->Start(FROM_HERE, kReportingUploadDeadline,
                          base::BindOnce(
                              [](Priority priority, int64_t generation_id) {
                                auto* const state =
                                    GetState(priority, generation_id);
                                state->job.reset();
                              },
                              priority, generation_id));

  // Store or increment upload counter for every event included in the upload.
  // `BuildPayload` included `events_to_send` events up to `last_sequence_id`
  // (inclusive); now we need to sample all events in
  // (last_sequence_id - events_to_send, last_sequence_id] range.
  while (events_to_send > 0u) {
    --events_to_send;
    // Set or increment uploads counter of the event.
    const auto [it, inserted] = state->upload_counters.try_emplace(
        last_sequence_id - events_to_send, 1u);
    if (!inserted) {
      ++(it->second);
    }
  }
}

void EncryptedReportingClient::OnReportUploadCompleted(
    Priority priority,
    int64_t generation_id,
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

  auto* const state = GetState(priority, generation_id);
  // Make sure the job is destruct by the end of this method.
  auto self_destruct_job = std::move(state->job);
  // Cancel timer.
  state->job_timer.reset();

  // Reset `last_sequence_id` in case any failure is detected.
  state->last_sequence_id = -1;

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
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::REPORT_CLIENT_BAD_RESPONSE_CODE,
        DataLossErrorReason::MAX_VALUE);
    return;
  }
  if (!response.has_value()) {
    std::move(callback).Run(base::unexpected(
        Status(error::DATA_LOSS, "Success response is empty")));
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::REPORT_CLIENT_EMPTY_RESPONSE,
        DataLossErrorReason::MAX_VALUE);
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

  // Invoke callbacks pending response.
  UploadResponseParser response_parser(GenerationGuidIsRequired(),
                                       std::move(response.value()));
  // Accept confirmation from the server.
  if (const auto last_sequence_info =
          response_parser.last_successfully_uploaded_record_sequence_info();
      last_sequence_info.has_value()) {
    base::UmaHistogramBoolean(kUmaRecordProcessedByServer, true);
    const int64_t last_sequence_id = last_sequence_info.value().sequencing_id();
    if (state->last_sequence_id < last_sequence_id ||
        response_parser.force_confirm_flag()) {
      state->last_sequence_id = last_sequence_id;
    }
    RemoveConfirmedEventsFromCache(state);
  }

  // Check if a record was unprocessable on the server.
  StatusOr<EncryptedRecord> failed_uploaded_record =
      response_parser.gap_record_for_permanent_failure();
  if (failed_uploaded_record.has_value()) {
    // The record we uploaded previously was unprocessable by the server.
    // Unless confirmation is flagged as `force`, upload the gap record.
    // Returns a gap record if it is necessary. Expects the contents of the
    // failedUploadedRecord field in the response:
    // {
    //   "sequencingId": 1234
    //   "generationId": 4321
    //   "priority": 3
    //   "generationGuid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
    // }
    // Gap record consists of an EncryptedRecord with just SequenceInformation.
    // The server will report success for the gap record and
    // `last_successfully_uploaded_record_sequence_info` will be updated in the
    // next response. In the future there may be recoverable `failureStatus`,
    // but for now all the device can do is skip the record.
    base::UmaHistogramBoolean(kUmaRecordProcessedByServer, false);
    const int64_t seq_id =
        failed_uploaded_record.value().sequence_information().sequencing_id();
    // If record is still cached, replace it by gap record.
    if (auto it = state->cached_records.find(seq_id);
        it != state->cached_records.end()) {
      // Replace by gap.
      it->second = std::move(failed_uploaded_record.value());
      // Reduce reserved memory.
      uint64_t records_memory = 0u;
      for (const auto& [_, record] : state->cached_records) {
        records_memory += record.ByteSizeLong();
      }
      state->scoped_reservation.Reduce(records_memory);
    }
  }

  // If failed upload is returned but is not parseable or does not match the
  // successfully uploaded part, just log an error.
  LOG_IF(ERROR, failed_uploaded_record.error().code() != error::NOT_FOUND)
      << failed_uploaded_record.error();

  // Forward results to the pending callback.
  std::move(callback).Run(std::move(response_parser));
}

// static
base::TimeDelta EncryptedReportingClient::WhenIsAllowedToProceed(
    Priority priority,
    int64_t generation_id) {
  // Retrieve state.
  const auto* const state = GetState(priority, generation_id);
  // If there are no records, allow upload (it will not overload the server).
  if (state->cached_records.empty()) {
    return base::TimeDelta();  // 0 - allowed right away.
  }

  // Use and update previously recorded state, base upload decision on it.
  if (state->last_sequence_id > state->cached_records.rbegin()->first) {
    // Sequence id decreased, the upload is outdated, reject it forever.
    return base::TimeDelta::Max();
  }
  if (priority == Priority::SECURITY) {
    // For SECURITY events the request is allowed.
    return base::TimeDelta();  // 0 - allowed right away.
  }

  // Allow upload only if earliest retry time has passed.
  // Return delta till the allowed time - if positive, upload is going to be
  // rejected.
  return state->earliest_retry_timestamp -
         state->backoff_entry->GetTimeTicksNow();
}

// static
void EncryptedReportingClient::AccountForAllowedJob(Priority priority,
                                                    int64_t generation_id,
                                                    int64_t last_sequence_id) {
  auto* const state = GetState(priority, generation_id);
  // Update state to reflect `last_sequence_id` (we never allow upload with
  // lower sequence_id).
  if (state->last_sequence_id < last_sequence_id) {
    state->last_sequence_id = last_sequence_id;
  }
  // Calculate delay as exponential backoff (based on the retry_count).
  // Update backoff under assumption that this request fails.
  // If it is responded successfully, we will reset it.
  state->backoff_entry->InformOfRequest(/*succeeded=*/false);
  state->earliest_retry_timestamp = state->backoff_entry->GetReleaseTime();
}

// static
void EncryptedReportingClient::AccountForUploadResponse(Priority priority,
                                                        int64_t generation_id,
                                                        int net_error,
                                                        int response_code) {
  // Analyze the net error and update upload state for possible future retries.
  auto* const state = GetState(priority, generation_id);
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

  timer_.Start(FROM_HERE, kReportingInterval,
               base::BindRepeating(&PayloadSizePerHourUmaReporter::Report,
                                   GetWeakPtr()));
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
