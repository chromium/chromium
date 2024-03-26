// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {
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
    bool is_generation_guid_required,
    bool attach_encryption_settings,
    int config_file_version)
    : is_generation_guid_required_(is_generation_guid_required) {
  result_.emplace();
  if (attach_encryption_settings) {
    result_->Set(GetAttachEncryptionSettingsPath(), true);
  }

  // Only attach the configuration file version to the request if the feature
  // is enabled. The server will only return the configuration file if there is
  // a mismatch between the version in the request and the version that it
  // holds.
  if (base::FeatureList::IsEnabled(kShouldRequestConfigurationFile)) {
    result_->Set(GetConfigurationFileVersionPath(), config_file_version);
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
      EncryptedRecordDictionaryBuilder(std::move(record), scoped_reservation,
                                       is_generation_guid_required_)
          .Build();
  if (!record_result.has_value()) {
    // Record has errors. Stop here.
    result_ = std::nullopt;
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

  result_->Set(reporting::json_keys::kRequestId, request_id);

  return *this;
}

std::optional<base::Value::Dict>
UploadEncryptedReportingRequestBuilder::Build() {
  if (result_.has_value()) {
    if (result_->empty()) {
      // Request is empty.
      return std::nullopt;
    }
    if (result_->size() == 1u &&
        result_->Find(reporting::json_keys::kRequestId) != nullptr) {
      // Nothing but request id in the request.
      return std::nullopt;
    }
    if (result_->FindString(reporting::json_keys::kRequestId) == nullptr) {
      CHECK(!result_->Find(reporting::json_keys::kRequestId))
          << "Non-string request id";
      SetRequestId(base::Token::CreateRandom().ToString());
    }
  }
  return std::move(result_);
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath() {
  return reporting::json_keys::kEncryptedRecordList;
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetAttachEncryptionSettingsPath() {
  return reporting::json_keys::kAttachEncryptionSettings;
}

// static
std::string_view
UploadEncryptedReportingRequestBuilder::GetConfigurationFileVersionPath() {
  return reporting::json_keys::kConfigurationFileVersion;
}

// static
std::string_view UploadEncryptedReportingRequestBuilder::GetSourcePath() {
  return reporting::json_keys::kSource;
}

EncryptedRecordDictionaryBuilder::EncryptedRecordDictionaryBuilder(
    EncryptedRecord record,
    ScopedReservation& scoped_reservation,
    bool is_generation_guid_required) {
  base::Value::Dict record_dictionary;

  // A record without sequence information cannot be uploaded - deny it.
  if (!record.has_sequence_information()) {
    return;
  }
  auto sequence_information_result =
      SequenceInformationDictionaryBuilder(record.sequence_information(),
                                           is_generation_guid_required)
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
    std::string base64_encode =
        base::Base64Encode(record.encrypted_wrapped_record());
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

std::optional<base::Value::Dict> EncryptedRecordDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetEncryptedWrappedRecordPath() {
  return json_keys::kEncryptedWrappedRecord;
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetSequenceInformationKeyPath() {
  return reporting::json_keys::kSequenceInformation;
}

// static
std::string_view EncryptedRecordDictionaryBuilder::GetEncryptionInfoPath() {
  return json_keys::kEncryptionInfo;
}

// static
std::string_view
EncryptedRecordDictionaryBuilder::GetCompressionInformationPath() {
  return json_keys::kCompressionInformation;
}

SequenceInformationDictionaryBuilder::SequenceInformationDictionaryBuilder(
    const SequenceInformation& sequence_information,
    bool is_generation_guid_required) {
  bool generation_guid_is_invalid = false;
#if BUILDFLAG(IS_CHROMEOS)
  if (is_generation_guid_required) {
    generation_guid_is_invalid = !sequence_information.has_generation_guid() ||
                                 sequence_information.generation_guid().empty();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // SequenceInformation requires these fields be set. `generation_guid` is
  // required only for unmanaged ChromeOS devices.
  if (!sequence_information.has_sequencing_id() ||
      !sequence_information.has_generation_id() ||
      !sequence_information.has_priority() || generation_guid_is_invalid) {
    return;
  }

  result_.emplace();
  result_->Set(GetSequencingIdPath(),
               base::NumberToString(sequence_information.sequencing_id()));
  result_->Set(GetGenerationIdPath(),
               base::NumberToString(sequence_information.generation_id()));
  result_->Set(GetPriorityPath(), sequence_information.priority());
#if BUILDFLAG(IS_CHROMEOS)
  result_->Set(GetGenerationGuidPath(), sequence_information.generation_guid());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

SequenceInformationDictionaryBuilder::~SequenceInformationDictionaryBuilder() =
    default;

std::optional<base::Value::Dict> SequenceInformationDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetSequencingIdPath() {
  return reporting::json_keys::kSequencingId;
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetGenerationIdPath() {
  return reporting::json_keys::kGenerationId;
}

// static
std::string_view SequenceInformationDictionaryBuilder::GetPriorityPath() {
  return reporting::json_keys::kPriority;
}

#if BUILDFLAG(IS_CHROMEOS)
// static
std::string_view SequenceInformationDictionaryBuilder::GetGenerationGuidPath() {
  return json_keys::kGenerationGuid;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

EncryptionInfoDictionaryBuilder::EncryptionInfoDictionaryBuilder(
    const EncryptionInfo& encryption_info) {
  base::Value::Dict encryption_info_dictionary;

  // EncryptionInfo requires both fields are set.
  if (!encryption_info.has_encryption_key() ||
      !encryption_info.has_public_key_id()) {
    return;
  }

  std::string base64_key = base::Base64Encode(encryption_info.encryption_key());
  encryption_info_dictionary.Set(GetEncryptionKeyPath(), base64_key);
  encryption_info_dictionary.Set(
      GetPublicKeyIdPath(),
      base::NumberToString(encryption_info.public_key_id()));
  result_ = std::move(encryption_info_dictionary);
}

EncryptionInfoDictionaryBuilder::~EncryptionInfoDictionaryBuilder() = default;

std::optional<base::Value::Dict> EncryptionInfoDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view EncryptionInfoDictionaryBuilder::GetEncryptionKeyPath() {
  return json_keys::kEncryptionKey;
}

// static
std::string_view EncryptionInfoDictionaryBuilder::GetPublicKeyIdPath() {
  return json_keys::kPublicKeyId;
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

std::optional<base::Value::Dict>
CompressionInformationDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
std::string_view
CompressionInformationDictionaryBuilder::GetCompressionAlgorithmPath() {
  return json_keys::kCompressionAlgorithm;
}

}  // namespace reporting
