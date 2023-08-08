// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_

#include <string_view>

#include "base/values.h"

#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

// {{{Note}}} ERP Encrypted Record
//
// Builds an upload request payload specific for
// EncryptedReportingJobConfiguration. A JSON version of the payload looks like
// this:
// {
//   "encryptedRecord": [
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "LocalPublicValue",
//         "publicKeyId": 1
//       },
//       "sequenceInformation": {
//         "sequencingId": 1,
//         "generationId": 123456789,
//         "priority": 1
//         // The string value of the `generation_guid` may be empty for managed
//         // devices, but will always have a value for unmanaged devices. It's
//         // value, if present, must be a string of base::Uuid. See base/uuid.h
//         // for format information.
//         "generation_guid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//       },
//       "compressionInformation": {
//         "compressionAlgorithm": 1
//       }
//     },
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "LocalPublicValue",
//         "publicKeyId": 2
//       },
//       "sequenceInformation": {
//         "sequencingId": 2,
//         "generationId": 123456789,
//         "priority": 1,
//         "generation_guid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//       },
//       "compressionInformation": {
//         "compressionAlgorithm": 1
//       }
//     }
//   ],
//   // optional field, corresponding to |need_encryption_keys| in
//   // components/reporting/proto/interface.proto
//   "attachEncryptionSettings": true,
//   "requestId": "SomeString"
// }
//
// This payload is added to the common payload of all reporting jobs, which
// includes other sub-fields such as "device" and "browser" (See note "ERP
// Payload Overview"):
//
//   EncryptedReportingRequestBuilder builder;
//   builder.AddRecord(record1);
//   builder.AddRecord(record2);
//   ...
//   builder.AddRecord(recordN);
//   auto payload_result = builder.Build();
//   CHECK(payload_result.has_value());
//   job_payload_.Merge(payload_result.value());
//
// The value of an "encryptedRecord" must be a list, in which each element is a
// dictionary that represents a record. The details of each record is documented
// in record.proto.

class UploadEncryptedReportingRequestBuilder {
 public:
  // SequenceInformationDictionaryBuilder strings
  static constexpr char kSequencingId[] = "sequencingId";
  static constexpr char kGenerationId[] = "generationId";
  static constexpr char kPriority[] = "priority";
  static constexpr char kGenerationGuid[] = "generationGuid";

  // RequestId key used to build UploadEncryptedReportingRequest
  static constexpr char kRequestId[] = "requestId";

  explicit UploadEncryptedReportingRequestBuilder(
      bool attach_encryption_settings = false);
  ~UploadEncryptedReportingRequestBuilder();

  // Adds record, converts it into base::Value::Dict, updates reservation to
  // reflect it (fails if unable to reserve).
  UploadEncryptedReportingRequestBuilder& AddRecord(
      EncryptedRecord record,
      ScopedReservation& reservation);

  // Sets the requestId field.
  UploadEncryptedReportingRequestBuilder& SetRequestId(
      std::string_view request_id);

  // Return the built dictionary. Also set requestId to a random string if it
  // hasn't been set yet.
  absl::optional<base::Value::Dict> Build();

  static std::string_view GetEncryptedRecordListPath();
  static std::string_view GetAttachEncryptionSettingsPath();

  absl::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |EncryptedRecord| proto.
class EncryptedRecordDictionaryBuilder {
 public:
  explicit EncryptedRecordDictionaryBuilder(
      EncryptedRecord record,
      ScopedReservation& scoped_reservation);
  ~EncryptedRecordDictionaryBuilder();

  absl::optional<base::Value::Dict> Build();

  static std::string_view GetEncryptedWrappedRecordPath();
  static std::string_view GetSequenceInformationKeyPath();
  static std::string_view GetEncryptionInfoPath();
  static std::string_view GetCompressionInformationPath();

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

  static std::string_view GetSequencingIdPath();
  static std::string_view GetGenerationIdPath();
  static std::string_view GetPriorityPath();
  static std::string_view GetGenerationGuidPath();

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

  static std::string_view GetEncryptionKeyPath();
  static std::string_view GetPublicKeyIdPath();

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

  static std::string_view GetCompressionAlgorithmPath();

 private:
  absl::optional<base::Value::Dict> result_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
