// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_

#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "net/base/backoff_entry.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace reporting {

// DmServerUploadService uploads events to the DMServer. It does not manage
// sequencing information, instead reporting the highest sequencing id for each
// generation id and priority.
//
// DmServerUploadService relies on DmServerUploader for uploading. A
// DmServerUploader is provided with RecordHandlers for each Destination. An
// |EnqueueUpload| call creates a DmServerUploader and provides it with the
// records for upload, and the RecordHandlers.  DmServerUploader uses the
// RecordHandlers to upload each record.
class DmServerUploadService {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this| (the respone consists of sequencing information and
  // force_confirm flag).
  using ReportSuccessfulUploadCallback =
      base::RepeatingCallback<void(SequencingInformation,
                                   /*force_confirm*/ bool)>;

  // ReceivedEncryptionKeyCallback is called if server attached encryption key
  // to the response.
  using EncryptionKeyAttachedCallback =
      base::RepeatingCallback<void(SignedEncryptionInfo)>;

  // Successful response consists of Sequencing information that may be
  // accompanied with force_confirm flag.
  struct SuccessfulUploadResponse {
    SequencingInformation sequencing_information;
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
    // highest accepted SequencingInformation, or an error detailing the failure
    // cause.
    // Any errors will result in |upload_complete| being called with a Status.
    virtual void HandleRecords(
        bool need_encryption_key,
        std::unique_ptr<std::vector<EncryptedRecord>> records,
        DmServerUploadService::CompletionCallback upload_complete,
        DmServerUploadService::EncryptionKeyAttachedCallback
            encryption_key_attached_cb) = 0;

   protected:
    explicit RecordHandler(policy::CloudPolicyClient* client);
    policy::CloudPolicyClient* GetClient() const { return client_; }

   private:
    policy::CloudPolicyClient* const client_;
  };

  // Context runner for handling the upload of events passed to the
  // DmServerUploadService. Will process records by verifying that they are
  // parseable and sending them to the appropriate handler.
  class DmServerUploader : public TaskRunnerContext<CompletionResponse> {
   public:
    DmServerUploader(
        bool need_encryption_key,
        std::unique_ptr<std::vector<EncryptedRecord>> records,
        RecordHandler* handler,
        CompletionCallback completion_cb,
        EncryptionKeyAttachedCallback encryption_key_attached_cb,
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

    // Called at the end of HandleRecords determines if all records have been
    // processed and calls Complete.
    void OnRecordsHandled();

    // Complete schedules |Response| with the provided |completion_response|.
    void Complete(CompletionResponse completion_response);

    // Helper function for determining if an EncryptedRecord is valid.
    Status IsRecordValid(const EncryptedRecord& encrypted_record,
                         const int64_t expected_generation_id,
                         const int64_t expected_sequencing_id) const;

    // Helper function for tracking the highest sequencing information per
    // generation id. Schedules ProcessSuccessfulUploadAddition.
    void AddSuccessfulUpload(
        base::RepeatingClosure done_cb,
        const SequencingInformation& sequencing_information);

    // Processes successful uploads on sequence.
    void ProcessSuccessfulUploadAddition(
        base::RepeatingClosure done_cb,
        SequencingInformation sequencing_information);

    const bool need_encryption_key_;
    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;
    EncryptionKeyAttachedCallback encryption_key_attached_cb_;
    RecordHandler* handler_;

    base::Optional<SequencingInformation> highest_successful_sequence_;

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
      policy::CloudPolicyClient* client,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);
  ~DmServerUploadService();

  Status EnqueueUpload(bool need_encryption_key,
                       std::unique_ptr<std::vector<EncryptedRecord>> record);

 private:
  DmServerUploadService(
      policy::CloudPolicyClient* client,
      ReportSuccessfulUploadCallback completion_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb);

  static void InitRecordHandler(
      std::unique_ptr<DmServerUploadService> uploader,
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);

  void UploadCompletion(CompletionResponse upload_result) const;

  policy::CloudPolicyClient* GetClient();

  policy::CloudPolicyClient* client_;
  ReportSuccessfulUploadCallback upload_cb_;
  EncryptionKeyAttachedCallback encryption_key_attached_cb_;
  std::unique_ptr<RecordHandler> handler_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
