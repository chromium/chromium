// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOADER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOADER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
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

// ReportSuccessfulUploadCallback is used to pass server responses back to
// the caller (the response consists of sequence information and force_confirm
// flag).
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

// Interface class for handling records to be sent to the server.
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
      CompletionCallback upload_complete,
      EncryptionKeyAttachedCallback encryption_key_attached_cb) = 0;

 protected:
  RecordHandler() = default;
};

// `DmServerUploader` uploads events to the DMServer. It is provided with
// `RecordHandler` instance owned by the caller, as well as with the flags and
// records for upload. Results are passed via callbacks.
// It processes records by verifying that they are parseable and sending them
// to the appropriate handler. Called from the `UploadClient`, so we can reuse
// the same sequence task runner.
class DmServerUploader : public TaskRunnerContext<CompletionResponse> {
 public:
  DmServerUploader(
      bool need_encryption_key,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      RecordHandler* handler,  // Not owned!
      ReportSuccessfulUploadCallback report_success_upload_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      CompletionCallback completion_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  DmServerUploader(const DmServerUploader&) = delete;
  DmServerUploader& operator=(const DmServerUploader&) = delete;

 private:
  ~DmServerUploader() override;

  // OnStart checks to ensure that our record set isn't empty, and requests
  // handler size status from |handlers_|.
  void OnStart() override;

  // ProcessRecords verifies that the records provided are parseable and sets
  // the |Record|s up for handling by the |RecordHandlers|s.
  // Returns OK if at least some records are ready fo upload, or error status
  // otherwise.
  Status ProcessRecords();

  // HandleRecords sends the records to the |record_handlers_|, allowing them
  // to upload to DmServer.
  void HandleRecords();

  // Processes |completion_response| and calls |Response|.
  void Finalize(CompletionResponse completion_response);

  // Helper function for determining if an EncryptedRecord is valid.
  Status IsRecordValid(const EncryptedRecord& encrypted_record,
                       const int64_t expected_generation_id,
                       const int64_t expected_sequencing_id) const;

  const bool need_encryption_key_;
  std::vector<EncryptedRecord> encrypted_records_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedReservation scoped_reservation_ GUARDED_BY_CONTEXT(sequence_checker_);
  const ReportSuccessfulUploadCallback report_success_upload_cb_;
  const EncryptionKeyAttachedCallback encryption_key_attached_cb_;
  // This dangling raw_ptr occurred in:
  // unit_tests: NeedOrNoNeedKey/DmServerUploaderTest.ReprotWithZeroRecords/2
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425192/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Aunit_tests%2FDmServerUploaderTest.ReprotWithZeroRecords%2FNeedOrNoNeedKey.2+VHash%3A728d3f3a440b40c1
  const raw_ptr<RecordHandler, FlakyDanglingUntriaged> handler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOADER_H_
