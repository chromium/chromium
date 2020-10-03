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
#include "chrome/browser/policy/messaging_layer/util/shared_vector.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "net/base/backoff_entry.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/profiles/profile.h"
#endif  // OS_CHROMEOS

namespace reporting {

// DmServerUploadService uploads events to the DMServer. It does not manage
// sequence information, instead reporting the highest sequence number for each
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
  // the owner of |this|.
  using ReportSuccessfulUploadCallback =
      base::RepeatingCallback<void(SequencingInformation)>;

  using CompletionResponse = StatusOr<SequencingInformation>;

  using CompletionCallback = base::OnceCallback<void(CompletionResponse)>;

  // Since DmServer records need to be sorted prior to sending, we need handlers
  // for each type of record.
  class RecordHandler {
   public:
    explicit RecordHandler(policy::CloudPolicyClient* client);
    virtual ~RecordHandler();

    virtual Status HandleRecord(Record record) = 0;

   protected:
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
        std::unique_ptr<std::vector<EncryptedRecord>> records,
        scoped_refptr<SharedVector<std::unique_ptr<RecordHandler>>> handlers,
        CompletionCallback completion_cb,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

   private:
    struct RecordInfo {
      Record record;
      SequencingInformation sequencing_information;
    };

    ~DmServerUploader() override;

    // OnStart checks to ensure that our record set isn't empty, and requests
    // handler size status from |handlers_|.
    void OnStart() override;

    // The callback for handler size status. Will early exit if there are no
    // available handlers. Otherwise schedules ProcessRecords.
    void IsHandlerVectorEmptyCheck(bool handler_is_empty);

    // ProcessRecords verifies that the records provided are parseable and sets
    // the |Record|s up for handling by the |RecordHandler|s. On completion,
    // ProcessRecords |Schedule|s |HandleRecords|.
    void ProcessRecords();

    // HandleRecords sends the records to the |record_handlers_|, allowing them
    // to upload to DmServer.
    void HandleRecords();

    // Called at the end of HandleRecords determines if all records have been
    // processed and calls Complete.
    void OnRecordsHandled();

    // Complete evaluates if any records were successfully uploaded.  If no
    // records were successfully uploaded and |status| is not ok - it calls
    // |Response| with the provided |status|. Otherwise it calls |Response| with
    // the list of successful uploads (even if some were not successful).
    void Complete(Status status);

    // Helper function for determining if a Record is valid and adding it to
    // |record_infos_|.
    Status IsRecordValid(const EncryptedRecord& encrypted_record);

    // Helper function for tracking the highest sequencing information per
    // generation id. Schedules ProcessSuccessfulUploadAddition.
    void AddSuccessfulUpload(
        base::RepeatingClosure done_cb,
        const SequencingInformation& sequencing_information);

    // Processes successful uploads on sequence.
    void ProcessSuccessfulUploadAddition(
        base::RepeatingClosure done_cb,
        SequencingInformation sequencing_information);

    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;
    const scoped_refptr<SharedVector<std::unique_ptr<RecordHandler>>> handlers_;

    // generation_id_ will be set to the generation of the first record in
    // encrypted_records_.
    uint64_t generation_id_;

    std::vector<RecordInfo> record_infos_;
    base::Optional<SequencingInformation> highest_successful_sequence_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Will asynchronously create a DMServerUploadService with handlers.
  // On successful completion will call |created_cb| with DMServerUploadService.
  // If |client| is null, will call |created_cb| with error::INVALID_ARGUMENT.
  // If any handlers fail to create, or the policy::CloudPolicyClient is null,
  // will call |created_cb| with error::UNAVAILABLE.
  //
  // |client| must not be null.
  // |report_upload_success_cb| should report back to the holder of the created
  // object whenever a record set is successfully uploaded.
  static void Create(
      std::unique_ptr<policy::CloudPolicyClient> client,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);
  ~DmServerUploadService();

  Status EnqueueUpload(std::unique_ptr<std::vector<EncryptedRecord>> record);

 private:
  DmServerUploadService(std::unique_ptr<policy::CloudPolicyClient> client,
                        ReportSuccessfulUploadCallback completion_cb);

  static void InitRecordHandlers(
      std::unique_ptr<DmServerUploadService> uploader,
#ifdef OS_CHROMEOS
      Profile* primary_profile,
#endif  // OS_CHROMEOS
      base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
          created_cb);

  void UploadCompletion(StatusOr<SequencingInformation>) const;

  policy::CloudPolicyClient* GetClient();

  std::unique_ptr<policy::CloudPolicyClient> client_;
  ReportSuccessfulUploadCallback upload_cb_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  scoped_refptr<SharedVector<std::unique_ptr<RecordHandler>>> record_handlers_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
