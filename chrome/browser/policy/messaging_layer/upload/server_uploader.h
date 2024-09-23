// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_SERVER_UPLOADER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_SERVER_UPLOADER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// UpdateConfigInMissiveCallback is called if the configuration file obtained
// from the server is different from the one that was sent previously using
// this callback.
using UpdateConfigInMissiveCallback =
    base::RepeatingCallback<void(ListOfBlockedDestinations)>;

// Successful response consists of Sequence information that may be
// accompanied with force_confirm flag.
struct SuccessfulUploadResponse {
  SequenceInformation sequence_information;
  bool force_confirm;
};
using CompletionResponse = StatusOr<SuccessfulUploadResponse>;

using CompletionCallback = base::OnceCallback<void(CompletionResponse)>;

// `ServerUploader` uploads events to the Reporting Server. It is provided with
// `RecordHandler` instance, as well as with the flags and records for upload.
// It processes records by verifying that they are parseable and sending them
// to the appropriate handler. Called from the `UploadClient`, so we can reuse
// the same sequence task runner.
// Results are passed via callbacks.
class ServerUploader : public TaskRunnerContext<CompletionResponse> {
 public:
  // Interface class for handling records to be sent to the server.
  class RecordHandler {
   public:
    virtual ~RecordHandler() = default;

    // Will iterate over `records` and ensure they are in ascending sequence
    // order, and within the same generation. Any out of order records will be
    // discarded. `need_encryption_key` is set to `true` if the client needs to
    // request the encryption key from the server (either because it does not
    // have it or because the one it has is old and may be outdated). In that
    // case it is ok for `records` to be empty (otherwise at least one record
    // must be present). If response has the key info attached, it is decoded
    // and handed over to `encryption_key_attached_cb`. Once the server has
    // responded `upload_complete` is called with either the highest accepted
    // SequenceInformation, or an error detailing the failure cause. Any errors
    // will result in `upload_complete` being called with a Status.
    virtual void HandleRecords(
        bool need_encryption_key,
        int config_file_version,
        std::vector<EncryptedRecord> records,
        ScopedReservation scoped_reservation,
        UploadEnqueuedCallback enqueued_cb,
        CompletionCallback upload_complete,
        EncryptionKeyAttachedCallback encryption_key_attached_cb,
        ConfigFileAttachedCallback config_file_attached_cb) = 0;

   protected:
    RecordHandler() = default;
  };

  ServerUploader(
      bool need_encryption_key,
      int config_file_version,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      std::unique_ptr<RecordHandler> handler,
      UploadEnqueuedCallback enqueued_cb,
      ReportSuccessfulUploadCallback report_success_upload_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb,
      CompletionCallback completion_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  ServerUploader(const ServerUploader&) = delete;
  ServerUploader& operator=(const ServerUploader&) = delete;

 private:
  ~ServerUploader() override;

  // OnStart checks to ensure that our record set isn't empty, and requests
  // handler size status from |handlers_|.
  void OnStart() override;

  // ProcessRecords verifies that the records provided are parseable and sets
  // the |Record|s up for handling by the |RecordHandlers|s.
  // Returns OK if at least some records are ready fo upload, or error status
  // otherwise.
  Status ProcessRecords();

  // HandleRecords sends the records to the |record_handlers_|, allowing them
  // to upload to Reporting Server.
  void HandleRecords();

  // Processes |completion_response| and calls |Response|.
  void Finalize(CompletionResponse completion_response);

  // Helper function for determining if an EncryptedRecord is valid.
  Status IsRecordValid(const EncryptedRecord& encrypted_record,
                       const int64_t expected_generation_id,
                       const int64_t expected_sequencing_id) const;

  const bool need_encryption_key_;
  const int config_file_version_;
  std::vector<EncryptedRecord> encrypted_records_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedReservation scoped_reservation_ GUARDED_BY_CONTEXT(sequence_checker_);
  UploadEnqueuedCallback enqueued_cb_;
  const ReportSuccessfulUploadCallback report_success_upload_cb_;
  const EncryptionKeyAttachedCallback encryption_key_attached_cb_;
  const ConfigFileAttachedCallback config_file_attached_cb_;
  const std::unique_ptr<RecordHandler> handler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_SERVER_UPLOADER_H_
