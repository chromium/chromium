// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"

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
//         // ChromeOS devices or any non-ChromeOS devices, but will always have
//         // a value for unmanaged ChromeOS devices. Its value, if present,
//         // must be a string of base::Uuid. See base/uuid.h for format
//         // information.
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
//   "requestId": "SomeString",
//   // optional field, corresponding to the configuration file version
//   // that the client is holding at the moment.
//   "configurationFileVersion": 1234
//   // optional field, only used by the client tast tests to signal to the
//   // server that this is an automated test from the lab. In production, this
//   // should always be absent. Even if it is erroneously present in production
//   // code, server ignores it. Marked as string to make it reusable in the
//   // future. Value should be "tast" in the tast tests.
//    "source": "SomeString"
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

BASE_DECLARE_FEATURE(kShouldRequestConfigurationFile);
BASE_DECLARE_FEATURE(kClientAutomatedTest);

class UploadEncryptedReportingRequestBuilder {
 public:
  // The default values signal the server that it shouldn't attach the
  // encryption settings and that the config_file_version hasn't been set by
  // `RecordHandlerImpl`.
  explicit UploadEncryptedReportingRequestBuilder(
      bool is_generation_guid_required,
      bool attach_encryption_settings = false,
      int config_file_version = -1);
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
  std::optional<base::Value::Dict> Build();

  static std::string_view GetEncryptedRecordListPath();
  static std::string_view GetAttachEncryptionSettingsPath();
  static std::string_view GetConfigurationFileVersionPath();
  static std::string_view GetSourcePath();

  const bool is_generation_guid_required_;
  std::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |EncryptedRecord| proto.
class EncryptedRecordDictionaryBuilder {
 public:
  explicit EncryptedRecordDictionaryBuilder(
      EncryptedRecord record,
      ScopedReservation& scoped_reservation,
      bool is_generation_guid_required);
  ~EncryptedRecordDictionaryBuilder();

  std::optional<base::Value::Dict> Build();

  static std::string_view GetEncryptedWrappedRecordPath();
  static std::string_view GetSequenceInformationKeyPath();
  static std::string_view GetEncryptionInfoPath();
  static std::string_view GetCompressionInformationPath();

 private:
  std::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |SequenceInformation| proto.
class SequenceInformationDictionaryBuilder {
 public:
  explicit SequenceInformationDictionaryBuilder(
      const SequenceInformation& sequence_information,
      bool is_generation_guid_required);
  ~SequenceInformationDictionaryBuilder();

  std::optional<base::Value::Dict> Build();

  static std::string_view GetSequencingIdPath();
  static std::string_view GetGenerationIdPath();
  static std::string_view GetPriorityPath();
#if BUILDFLAG(IS_CHROMEOS)
  static std::string_view GetGenerationGuidPath();
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  std::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |EncryptionInfo| proto.
class EncryptionInfoDictionaryBuilder {
 public:
  explicit EncryptionInfoDictionaryBuilder(
      const EncryptionInfo& encryption_info);
  ~EncryptionInfoDictionaryBuilder();

  std::optional<base::Value::Dict> Build();

  static std::string_view GetEncryptionKeyPath();
  static std::string_view GetPublicKeyIdPath();

 private:
  std::optional<base::Value::Dict> result_;
};

// Builds a |base::Value::Dict| from a |CompressionInfo| proto.
class CompressionInformationDictionaryBuilder {
 public:
  explicit CompressionInformationDictionaryBuilder(
      const CompressionInformation& compression_info);
  ~CompressionInformationDictionaryBuilder();

  std::optional<base::Value::Dict> Build();

  static std::string_view GetCompressionAlgorithmPath();

 private:
  std::optional<base::Value::Dict> result_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_UPLOAD_REQUEST_BUILDER_H_
