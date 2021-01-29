// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/reporting/proto/record.pb.h"

namespace reporting {

// Builds an upload request payload specific for
// EncryptedReportingJobConfiguration A JSON version of the payload looks like
// this:
// {
//   "encryptedRecord": [
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "EncryptedMessage",
//         "publicKeyId": 1
//       },
//       "sequencingInformation": {
//         "sequencingId": 1,
//         "generationId": 123456789,
//         "priority": 1
//       }
//       "sequenceInformation": {
//         "sequencingId": 1,
//         "generationId": 123456789,
//         "priority": 1
//       }
//     },
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "EncryptedMessage",
//         "publicKeyId": 2
//       },
//       "sequencingInformation": {
//         "sequencingId": 2,
//         "generationId": 123456789,
//         "priority": 1
//       }
//       "sequenceInformation": {
//         "sequencingId": 2,
//         "generationId": 123456789,
//         "priority": 1
//       }
//     }
//   ]
//   "attachEncryptionSettings": true  // optional field
// }
// TODO(b/159361496): Periodically add memory and disk space usage.
//
// Note that there are two identical sub-records - sequencingInformation and
// sequenceInformation (sequencingId and generationId in the former are
// Unsigned, in the later - Signed). This is done temporarily for backwards
// compatibility with the server.
// TODO(b/177677467): Remove this duplication once server is fully transitioned.
//
// This payload is added to the common payload of all reporting jobs, which
// includes "device" and "browser" sub-fields:
//
// EncryptedReportingRequestBuilder builder;
// builder.AddRecord(record1);
// builder.AddRecord(record2);
//  ...
// builder.AddRecord(recordN);
// auto payload_result = builder.Build();
// DCHECK(payload_result.has_value());
// job_payload_.MergeDict(payload_result.value());

class UploadEncryptedReportingRequestBuilder {
 public:
  explicit UploadEncryptedReportingRequestBuilder(
      bool attach_encryption_settings = false);
  ~UploadEncryptedReportingRequestBuilder();

  // TODO(chromium:1165908) Have AddRecord take ownership of the record that is
  // passed in.
  UploadEncryptedReportingRequestBuilder& AddRecord(
      const EncryptedRecord& record);

  base::Optional<base::Value> Build();

  static base::StringPiece GetEncryptedRecordListPath();
  static base::StringPiece GetAttachEncryptionSettingsPath();

  static const char kEncryptedRecordListKey_[];

  base::Optional<base::Value> result_;
};

// Builds a |base::Value| dictionary from a |EncryptedRecord|
// proto.
class EncryptedRecordDictionaryBuilder {
 public:
  explicit EncryptedRecordDictionaryBuilder(const EncryptedRecord& record);
  ~EncryptedRecordDictionaryBuilder();

  base::Optional<base::Value> Build();

  static base::StringPiece GetEncryptedWrappedRecordPath();
  static base::StringPiece GetUnsignedSequencingInformationKeyPath();
  static base::StringPiece GetSequencingInformationKeyPath();
  static base::StringPiece GetEncryptionInfoPath();

 private:
  base::Optional<base::Value> result_;
};

// Builds a |base::Value| dictionary from a |SequencingInformation|
// proto.
class SequencingInformationDictionaryBuilder {
 public:
  explicit SequencingInformationDictionaryBuilder(
      const SequencingInformation& sequencing_information);
  ~SequencingInformationDictionaryBuilder();

  base::Optional<base::Value> Build();

  static base::StringPiece GetSequencingIdPath();
  static base::StringPiece GetGenerationIdPath();
  static base::StringPiece GetPriorityPath();

 private:
  base::Optional<base::Value> result_;
};

// Builds a |base::Value| dictionary from a |EncryptionInfo| proto.
class EncryptionInfoDictionaryBuilder {
 public:
  explicit EncryptionInfoDictionaryBuilder(
      const EncryptionInfo& encryption_info);
  ~EncryptionInfoDictionaryBuilder();

  base::Optional<base::Value> Build();

  static base::StringPiece GetEncryptionKeyPath();
  static base::StringPiece GetPublicKeyIdPath();

 private:
  base::Optional<base::Value> result_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
