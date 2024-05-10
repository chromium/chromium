// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_

#include <list>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Implements the logic required to talk to the device management service
// for Encrypted Reporting Pipeline records upload.
class EncryptedReportingClient {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual policy::DeviceManagementService* device_management_service() const;
  };

  // Reports accumulated payload sizes per hour via UMA.
  class PayloadSizePerHourUmaReporter {
   public:
    PayloadSizePerHourUmaReporter();
    ~PayloadSizePerHourUmaReporter();
    PayloadSizePerHourUmaReporter(const PayloadSizePerHourUmaReporter&) =
        delete;
    PayloadSizePerHourUmaReporter& operator=(
        const PayloadSizePerHourUmaReporter&) = delete;

    // Adds request payload size to the accumulated request payload size.
    void RecordRequestPayloadSize(int payload_size);

    // Adds response payload size to the accumulated response payload size.
    void RecordResponsePayloadSize(int payload_size);

    // Gets the weak pointer.
    base::WeakPtr<PayloadSizePerHourUmaReporter> GetWeakPtr();

   private:
    // Reporting interval.
    static constexpr base::TimeDelta kReportingInterval = base::Hours(1);

    // Converts bytes to KiB.
    static int ConvertBytesToKiB(int bytes);

    // Reports the data to UMA.
    void Report();

    // Accumulated request payload size since last report.
    int request_payload_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    // Accumulated response payload size since last report.
    int response_payload_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    // Timer that controls when network usage is reported.
    base::RepeatingTimer timer_;

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtrFactory<PayloadSizePerHourUmaReporter> weak_factory_{this};
  };

  using ResponseCallback =
      base::OnceCallback<void(StatusOr<UploadResponseParser>)>;

  // Server is expected to respond within this time, otherwise the upload
  // job is cancelled and the data will be re-uploaded as soon as the throttling
  // permits.
  static constexpr base::TimeDelta kReportingUploadDeadline = base::Minutes(2);

  static std::unique_ptr<EncryptedReportingClient> Create(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());

  EncryptedReportingClient(const EncryptedReportingClient&) = delete;
  EncryptedReportingClient& operator=(const EncryptedReportingClient&) = delete;

  ~EncryptedReportingClient();

  // Returns true if a generation guid is required for this device or browser.
  // Returns false otherwise.
  static bool GenerationGuidIsRequired();

  // Presets common settings to be applied to future cached uploads. May be
  // called more than once, but settings are expected to end up being the same.
  void PresetUploads(base::Value::Dict context,
                     std::string dm_token,
                     std::string client_id);

  // Uploads a report containing multiple `records`, augmented with
  // `need_encryption_key` flag and `config_file_version`. Calls `callback` when
  // the upload process is completed. Uses `scoped_reservation` to ensure proper
  // memory management (stops and returns error if memory is insufficient).
  void UploadReport(bool need_encryption_key,
                    int config_file_version,
                    std::vector<EncryptedRecord> records,
                    ScopedReservation scoped_reservation,
                    UploadEnqueuedCallback enqueued_cb,
                    ResponseCallback callback);

  // Test-only method that resets collected uploads state.
  static void ResetUploadsStateForTest();

 private:
  friend class EncryptedReportingClientTest;
  FRIEND_TEST_ALL_PREFIXES(EncryptedReportingClientTest,
                           IdenticalUploadRetriesThrottled);
  FRIEND_TEST_ALL_PREFIXES(EncryptedReportingClientTest,
                           UploadsSequenceThrottled);
  FRIEND_TEST_ALL_PREFIXES(EncryptedReportingClientTest,
                           SecurityUploadsSequenceNotThrottled);
  FRIEND_TEST_ALL_PREFIXES(EncryptedReportingClientTest,
                           FailedUploadsSequenceThrottled);

  // Constructor called by factory only.
  explicit EncryptedReportingClient(std::unique_ptr<Delegate> delegate);

  // Performs actual upload unless one is already in flight (calls `callback`
  // with error in that case).
  void MaybePerformUpload(bool need_encryption_key,
                          int config_file_version,
                          Priority priority,
                          int64_t generation_id,
                          ResponseCallback callback);

  // Constructs upload job after the data is converted into JSON, assigned to
  // `payload_result` (`nullopt` if there was an error). Calls `callback` once
  // the job has been responded or if an error has been detected, and releases
  // `scoped_reservation`.
  void CreateUploadJob(
      Priority priority,
      int64_t generation_id,
      policy::EncryptedReportingJobConfiguration::UploadResponseCallback
          response_cb,
      ResponseCallback callback,
      std::optional<base::Value::Dict> payload_result,
      ScopedReservation scoped_reservation,
      int64_t last_sequence_id,
      uint64_t events_to_send);

  // Callback for encrypted report upload requests.
  void OnReportUploadCompleted(Priority priority,
                               int64_t generation_id,
                               ScopedReservation scoped_reservation,
                               std::optional<int> request_payload_size,
                               base::WeakPtr<PayloadSizePerHourUmaReporter>
                                   payload_size_per_hour_uma_reporter,
                               ResponseCallback callback,
                               policy::DeviceManagementService::Job* job,
                               policy::DeviceManagementStatus status,
                               int response_code,
                               std::optional<base::Value::Dict> response);

  // Checks the new job against the history, determines how soon the upload will
  // be allowed. Returns positive value if not allowed, and 0 or negative
  // otherwise.
  static base::TimeDelta WhenIsAllowedToProceed(Priority priority,
                                                int64_t generation_id);

  // Account for the job, that was allowed to proceed.
  static void AccountForAllowedJob(Priority priority,
                                   int64_t generation_id,
                                   int64_t last_sequence_id);

  // Accounts for net error and response code of the upload.
  static void AccountForUploadResponse(Priority priority,
                                       int64_t generation_id,
                                       int net_error,
                                       int response_code);

  SEQUENCE_CHECKER(sequence_checker_);

  // Cached elements expected by the reporting server.
  std::string dm_token_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string client_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Value::Dict context_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::unique_ptr<Delegate> delegate_;

  // Reports accumulated payload sizes per hour via UMA.
  PayloadSizePerHourUmaReporter payload_size_per_hour_uma_reporter_;

  base::WeakPtrFactory<EncryptedReportingClient> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_
