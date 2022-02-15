// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
// job_payload_.Merge(payload_result.value());

class UploadEncryptedReportingRequestBuilder {
 public:
  // RequestId key used to build UploadEncryptedReportingRequest
  static constexpr char kRequestId[] = "requestId";

  explicit UploadEncryptedReportingRequestBuilder(
      bool attach_encryption_settings = false);
  ~UploadEncryptedReportingRequestBuilder();

  UploadEncryptedReportingRequestBuilder& AddRecord(EncryptedRecord record);

  UploadEncryptedReportingRequestBuilder& SetRequestId(
      base::StringPiece request_id);

  absl::optional<base::Value::Dict> Build();

  static base::StringPiece GetEncryptedRecordListPath();
  static base::StringPiece GetAttachEncryptionSettingsPath();

  static const char kEncryptedRecordListKey_[];

  absl::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |EncryptedRecord| proto.
class EncryptedRecordDictionaryBuilder {
 public:
  explicit EncryptedRecordDictionaryBuilder(EncryptedRecord record);
  ~EncryptedRecordDictionaryBuilder();

  absl::optional<base::Value::Dict> Build();

  static base::StringPiece GetEncryptedWrappedRecordPath();
  static base::StringPiece GetSequenceInformationKeyPath();
  static base::StringPiece GetEncryptionInfoPath();
  static base::StringPiece GetCompressionInformationPath();

 private:
  absl::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |SequenceInformation| proto.
class SequenceInformationDictionaryBuilder {
 public:
  explicit SequenceInformationDictionaryBuilder(
      const SequenceInformation& sequence_information);
  ~SequenceInformationDictionaryBuilder();

  absl::optional<base::Value::Dict> Build();

  static base::StringPiece GetSequencingIdPath();
  static base::StringPiece GetGenerationIdPath();
  static base::StringPiece GetPriorityPath();

 private:
  absl::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |EncryptionInfo| proto.
class EncryptionInfoDictionaryBuilder {
 public:
  explicit EncryptionInfoDictionaryBuilder(
      const EncryptionInfo& encryption_info);
  ~EncryptionInfoDictionaryBuilder();

  absl::optional<base::Value::Dict> Build();

  static base::StringPiece GetEncryptionKeyPath();
  static base::StringPiece GetPublicKeyIdPath();

 private:
  absl::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |CompressionInfo| proto.
class CompressionInformationDictionaryBuilder {
 public:
  explicit CompressionInformationDictionaryBuilder(
      const CompressionInformation& compression_info);
  ~CompressionInformationDictionaryBuilder();

  absl::optional<base::Value::Dict> Build();

  static base::StringPiece GetCompressionAlgorithmPath();

 private:
  absl::optional<base::Value::Dict> result_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
