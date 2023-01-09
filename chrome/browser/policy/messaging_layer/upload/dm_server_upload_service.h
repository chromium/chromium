// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// DmServerUploadService uploads events to the DMServer. It does not manage
// sequence information, instead reporting the highest sequencing id for each
// generation id and priority.
//
// DmServerUploadService relies on DmServerUploader for uploading. A
// DmServerUploader is provided with RecordHandlers for each Destination. An
// |EnqueueUpload| call creates a DmServerUploader and provides it with the
// flags and records for upload and the result handlers.  DmServerUploader uses
// the RecordHandlers to upload each record.
class DmServerUploadService {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this| (the respone consists of sequence information and
  // force_confirm flag).
  using ReportSuccessfulUploadCallback =
      base::RepeatingCallback<void(SequenceInformation,
                                   /*force_confirm*/ bool)>;

  // ReceivedEncryptionKeyCallback is called if server attached encryption key
  // to the response.
  using EncryptionKeyAttachedCallback =
      base::RepeatingCallback<void(SignedEncryptionInfo)>;

  // Successful response consists of Sequence information that may be
  // accompanied with force_confirm flag.
  struct SuccessfulUploadResponse {
    SequenceInformation sequence_information;
    bool force_confirm;
  };
  using CompletionResponse = StatusOr<SuccessfulUploadResponse>;

  using CompletionCallback = base::OnceCallback<void(CompletionResponse)>;

  // Handles sending records to the server.
  class RecordHandler {
   public:
    virtual ~RecordHandler() = default;

    // Will iterate over |records| and ensure they are in ascending sequence
    // order, and within the same generation. Any out of order records will be
    // discarded.
    // |need_encryption_key| is set to `true` if the client needs to request
    // the encryption key from the server (either because it does not have it
    // or because the one it has is old and may be outdated). In that case
    // it is ok for |records| to be empty (otherwise at least one record must
    // be present). If response has the key info attached, it is decoded and
    // handed over to |encryption_key_attached_cb|.
    // Once the server has responded |upload_complete| is called with either the
    // highest accepted SequenceInformation, or an error detailing the failure
    // cause.
    // Any errors will result in |upload_complete| being called with a Status.
    virtual void HandleRecords(
        bool need_encryption_key,
        std::vector<EncryptedRecord> records,
        ScopedReservation scoped_reservation,
        DmServerUploadService::CompletionCallback upload_complete,
        DmServerUploadService::EncryptionKeyAttachedCallback
            encryption_key_attached_cb) = 0;

   protected:
    RecordHandler();
  };

  // Context runner for handling the upload of events passed to the
  // DmServerUploadService. Will process records by verifying that they are
  // parseable and sending them to the appropriate handler.
  class DmServerUploader : public TaskRunnerContext<CompletionResponse> {
   public:
    DmServerUploader(
        bool need_encryption_key,
        std::vector<EncryptedRecord> records,
        ScopedReservation scoped_reservation,
        RecordHandler* handler,
        ReportSuccessfulUploadCallback report_success_upload_cb,
        EncryptionKeyAttachedCallback encryption_key_attached_cb,
        CompletionCallback completion_cb,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

   private:
    ~DmServerUploader() override;

    // OnStart checks to ensure that our record set isn't empty, and requests
    // handler size status from |handlers_|.
    void OnStart() override;

    // ProcessRecords verifies that the records provided are parseable and sets
    // the |Record|s up for handling by the |RecordHandlers|s. On
    // completion, ProcessRecords |Schedule|s |HandleRecords|.
    void ProcessRecords();

    // HandleRecords sends the records to the |record_handlers_|, allowing them
    // to upload to DmServer.
    void HandleRecords();

    // Processes |completion_response| and call |Response|.
    void Finalize(CompletionResponse completion_response);

    // Complete schedules |Finalize| with the provided |completion_response|.
    void Complete(CompletionResponse completion_response);

    // Helper function for determining if an EncryptedRecord is valid.
    Status IsRecordValid(const EncryptedRecord& encrypted_record,
                         const int64_t expected_generation_id,
                         const int64_t expected_sequencing_id) const;

    const bool need_encryption_key_;
    std::vector<EncryptedRecord> encrypted_records_;
    ScopedReservation scoped_reservation_;
    const ReportSuccessfulUploadCallback report_success_upload_cb_;
    const EncryptionKeyAttachedCallback encryption_key_attached_cb_;
    raw_ptr<RecordHandler> handler_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Will asynchronously create a DMServerUploadService with handlers.
  // On successful completion will call |created_cb| with DMServerUploadService.
  // If |client| is null, will call |created_cb| with error::INVALID_ARGUMENT.
  // If any handlers fail to create, will call |created_cb| with
  // error::UNAVAILABLE.
  //
  // |report_upload_success_cb| should report back to the holder of the created
  // object whenever a record set is successfully uploaded.
  // |encryption_key_attached_cb| if called would update the encryption key with
  // the one received from the server.
  static void Create(
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);
  ~DmServerUploadService();

  Status EnqueueUpload(
      bool need_encryption_key,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb);

 private:
  DmServerUploadService();

  static void InitRecordHandler(
      std::unique_ptr<DmServerUploadService> uploader,
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);

  std::unique_ptr<RecordHandler> handler_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
