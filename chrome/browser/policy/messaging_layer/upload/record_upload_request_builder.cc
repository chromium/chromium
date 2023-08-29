// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {

// UploadEncryptedReportingRequestBuilder list key
constexpr char kEncryptedRecordListKey[] = "encryptedRecord";
constexpr char kAttachEncryptionSettingsKey[] = "attachEncryptionSettings";
constexpr char kAttachConfigurationFile[] = "attachConfigurationFile";
constexpr char kSourcePath[] = "source";

// EncryptedRecordDictionaryBuilder strings
constexpr char kEncryptedWrappedRecord[] = "encryptedWrappedRecord";
constexpr char kSequenceInformationKey[] = "sequenceInformation";
constexpr char kEncryptionInfoKey[] = "encryptionInfo";
constexpr char kCompressionInformationKey[] = "compressionInformation";

// SequenceInformationDictionaryBuilder strings located in header file.

// EncryptionInfoDictionaryBuilder strings
constexpr char kEncryptionKey[] = "encryptionKey";
constexpr char kPublicKeyId[] = "publicKeyId";

// CompressionInformationDictionaryBuilder strings
constexpr char kCompressionAlgorithmKey[] = "compressionAlgorithm";

}  // namespace

// Feature that controls if the configuration file should be requested
// from the server.
BASE_FEATURE(kShouldRequestConfigurationFile,
             "ShouldRequestConfigurationFile",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used in the tast tests to let the server know that the events are
// coming from an automated client test. Only used in tast tests.
BASE_FEATURE(kClientAutomatedTest,
             "ClientAutomatedTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

UploadEncryptedReportingRequestBuilder::UploadEncryptedReportingRequestBuilder(
    bool attach_encryption_settings) {
  result_.emplace();
  if (attach_encryption_settings) {
    result_->Set(GetAttachEncryptionSettingsPath(), true);
  }

  // Only request the configuration file from the server if the feature is
  // enabled. The server will only return the configuration file if there is
  // something to be blocked at that point.s
  if (base::FeatureList::IsEnabled(kShouldRequestConfigurationFile)) {
    result_->Set(GetAttachConfigurationFilePath(), true);
  }

  // This feature signals the server that this is an automated client test.
  if (base::FeatureList::IsEnabled(kClientAutomatedTest)) {
    result_->Set(GetSourcePath(), "tast");
  }
}

UploadEncryptedReportingRequestBuilder::
    ~UploadEncryptedReportingRequestBuilder() = default;

UploadEncryptedReportingRequestBuilder&
UploadEncryptedReportingRequestBuilder::AddRecord(
    EncryptedRecord record,
    ScopedReservation& scoped_reservation) {
  if (!result_.has_value()) {
    // Some errors were already detected.
    return *this;
  }
  base::Value::List* records_list =
      result_->FindList(GetEncryptedRecordListPath());
  if (!records_list) {
    records_list =
        &result_->Set(GetEncryptedRecordListPath(), base::Value::List())
             ->GetList();
  }

  auto record_result =
      EncryptedRecordDictionaryBuilder(std::move(record), scoped_reservation)
          .Build();
  if (!record_result.has_value()) {
    // Record has errors. Stop here.
    result_ = absl::nullopt;
    return *this;
  }

  records_list->Append(std::move(record_result.value()));
  return *this;
}

UploadEncryptedReportingRequestBuilder&
UploadEncryptedReportingRequestBuilder::SetRequestId(
    std::string_view request_id) {
  if (!result_.has_value()) {
    // Some errors were already detected
    return *this;
  }

  result_->Set(UploadEncryptedReportingRequestBuilder::kRequestId, request_id);

  return *this;
}

absl::optional<base::Value::Dict>
UploadEncryptedReportingRequestBuilder::Build() {
  // Ensure that if result_ has value, then it must not have a non-string
  // requestId.
  CHECK(!(result_.has_value() &&
          result_->Find(UploadEncryptedReportingRequestBuilder::kRequestId) &&
          !result_->FindString(
              UploadEncryptedReportingRequestBuilder::kRequestId)));
  if (result_.has_value() &&
      result_->FindString(UploadEncryptedReportingRequestBuilder::kRequestId) ==
          nullptr) {
    SetRequestId(base::Token::CreateRandom().ToString());
  }
  return std::move(result_);
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath() {
  return kEncryptedRecordListKey;
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetAttachEncryptionSettingsPath() {
  return kAttachEncryptionSettingsKey;
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetAttachConfigurationFilePath() {
  return kAttachConfigurationFile;
}

// static
std::string_view UploadEncryptedReportingRequestBuilder::GetSourcePath() {
  return kSourcePath;
}

EncryptedRecordDictionaryBuilder::EncryptedRecordDictionaryBuilder(
    EncryptedRecord record,
    ScopedReservation& scoped_reservation) {
  base::Value::Dict record_dictionary;

  // A record without sequence information cannot be uploaded - deny it.
  if (!record.has_sequence_information()) {
    return;
  }
  auto sequence_information_result =
      SequenceInformationDictionaryBuilder(record.sequence_information())
          .Build();
  if (!sequence_information_result.has_value()) {
    // Sequencing information was improperly configured. Record cannot be
    // uploaded. Deny it.
    return;
  }
  record_dictionary.Set(GetSequenceInformationKeyPath(),
                        std::move(sequence_information_result.value()));

  // Encryption information can be missing until we set up encryption as
  // mandatory.
  if (record.has_encryption_info()) {
    auto encryption_info_result =
        EncryptionInfoDictionaryBuilder(record.encryption_info()).Build();
    if (!encryption_info_result.has_value()) {
      // Encryption info has been corrupted or set improperly. Deny it.
      return;
    }
    record_dictionary.Set(GetEncryptionInfoPath(),
                          std::move(encryption_info_result.value()));
  }

  // Compression information can be missing until we set up compression as
  // mandatory.
  if (record.has_compression_information()) {
    auto compression_information_result =
        CompressionInformationDictionaryBuilder(
            record.compression_information())
            .Build();
    if (!compression_information_result.has_value()) {
      // Compression info has been corrupted or set improperly. Deny it.
      return;
    }
    record_dictionary.Set(GetCompressionInformationPath(),
                          std::move(compression_information_result.value()));
  }

  // Gap records won't fill in this field, so it can be missing.
  if (record.has_encrypted_wrapped_record()) {
    std::string base64_encode;
    base::Base64Encode(record.encrypted_wrapped_record(), &base64_encode);
    ScopedReservation base64_encode_reservation(base64_encode.size(),
                                                scoped_reservation);
    if (!base64_encode_reservation.reserved()) {
      // Insufficient memory
      return;
    }
    record_dictionary.Set(GetEncryptedWrappedRecordPath(),
                          std::move(base64_encode));
    // Replace record reservation with base64_encode.
    scoped_reservation.Reduce(0uL);
    scoped_reservation.HandOver(base64_encode_reservation);
  }

  // Result complete.
  result_ = std::move(record_dictionary);
}

EncryptedRecordDictionaryBuilder::~EncryptedRecordDictionaryBuilder() = default;

absl::optional<base::Value::Dict> EncryptedRecordDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetEncryptedWrappedRecordPath() {
  return kEncryptedWrappedRecord;
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetSequenceInformationKeyPath() {
  return kSequenceInformationKey;
}

// static
std::string_view EncryptedRecordDictionaryBuilder::GetEncryptionInfoPath() {
  return kEncryptionInfoKey;
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetCompressionInformationPath() {
  return kCompressionInformationKey;
}

SequenceInformationDictionaryBuilder::SequenceInformationDictionaryBuilder(
    const SequenceInformation& sequence_information) {
  // SequenceInformation requires these fields be set. `generation_guid` is
  // required only for unmanaged devices.
  if (!sequence_information.has_sequencing_id() ||
      !sequence_information.has_generation_id() ||
      !sequence_information.has_priority() ||
      // Require generation guid for non ChromeOS-managed devices.
      (!policy::ManagementServiceFactory::GetForPlatform()
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) &&
       !sequence_information.has_generation_guid())) {
    return;
  }

  result_.emplace();
  result_->Set(GetSequencingIdPath(),
               base::NumberToString(sequence_information.sequencing_id()));
  result_->Set(GetGenerationIdPath(),
               base::NumberToString(sequence_information.generation_id()));
  result_->Set(GetPriorityPath(), sequence_information.priority());
  result_->Set(GetGenerationGuidPath(), sequence_information.generation_guid());
}

SequenceInformationDictionaryBuilder::~SequenceInformationDictionaryBuilder() =
    default;

absl::optional<base::Value::Dict>
SequenceInformationDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetSequencingIdPath() {
  return UploadEncryptedReportingRequestBuilder::kSequencingId;
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetGenerationIdPath() {
  return UploadEncryptedReportingRequestBuilder::kGenerationId;
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetPriorityPath() {
  return UploadEncryptedReportingRequestBuilder::kPriority;
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetGenerationGuidPath() {
  return UploadEncryptedReportingRequestBuilder::kGenerationGuid;
}

EncryptionInfoDictionaryBuilder::EncryptionInfoDictionaryBuilder(
    const EncryptionInfo& encryption_info) {
  base::Value::Dict encryption_info_dictionary;

  // EncryptionInfo requires both fields are set.
  if (!encryption_info.has_encryption_key() ||
      !encryption_info.has_public_key_id()) {
    return;
  }

  std::string base64_key;
  base::Base64Encode(encryption_info.encryption_key(), &base64_key);
  encryption_info_dictionary.Set(GetEncryptionKeyPath(), base64_key);
  encryption_info_dictionary.Set(
      GetPublicKeyIdPath(),
      base::NumberToString(encryption_info.public_key_id()));
  result_ = std::move(encryption_info_dictionary);
}

EncryptionInfoDictionaryBuilder::~EncryptionInfoDictionaryBuilder() = default;

absl::optional<base::Value::Dict> EncryptionInfoDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view EncryptionInfoDictionaryBuilder::GetEncryptionKeyPath() {
  return kEncryptionKey;
}

// static
std::string_view EncryptionInfoDictionaryBuilder::GetPublicKeyIdPath() {
  return kPublicKeyId;
}

CompressionInformationDictionaryBuilder::
    CompressionInformationDictionaryBuilder(
        const CompressionInformation& compression_information) {
  base::Value::Dict compression_information_dictionary;

  // Ensure that compression_algorithm is valid.
  if (!CompressionInformation::CompressionAlgorithm_IsValid(
          compression_information.compression_algorithm())) {
    return;
  }

  compression_information_dictionary.Set(
      GetCompressionAlgorithmPath(),
      compression_information.compression_algorithm());
  result_ = std::move(compression_information_dictionary);
}

CompressionInformationDictionaryBuilder::
    ~CompressionInformationDictionaryBuilder() = default;

absl::optional<base::Value::Dict>
CompressionInformationDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view
CompressionInformationDictionaryBuilder::GetCompressionAlgorithmPath() {
  return kCompressionAlgorithmKey;
}

}  // namespace reporting
