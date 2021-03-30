// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"

#include <string>

#include "base/base64.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"

namespace reporting {

namespace {

// UploadEncryptedReportingRequestBuilder list key
constexpr char kEncryptedRecordListKey[] = "encryptedRecord";
constexpr char kAttachEncryptionSettingsKey[] = "attachEncryptionSettings";

// EncrypedRecordDictionaryBuilder strings
constexpr char kEncryptedWrappedRecord[] = "encryptedWrappedRecord";
constexpr char kUnsignedSequencingInformationKey[] = "sequencingInformation";
constexpr char kSequencingInformationKey[] = "sequenceInformation";
constexpr char kEncryptionInfoKey[] = "encryptionInfo";

// SequencingInformationDictionaryBuilder strings
constexpr char kSequencingId[] = "sequencingId";
constexpr char kGenerationId[] = "generationId";
constexpr char kPriority[] = "priority";

// EncryptionInfoDictionaryBuilder strings
constexpr char kEncryptionKey[] = "encryptionKey";
constexpr char kPublicKeyId[] = "publicKeyId";

}  // namespace

UploadEncryptedReportingRequestBuilder::UploadEncryptedReportingRequestBuilder(
    bool attach_encryption_settings) {
  result_ = base::Value{base::Value::Type::DICTIONARY};
  result_.value().SetKey(GetEncryptedRecordListPath(),
                         base::Value{base::Value::Type::LIST});
  if (attach_encryption_settings) {
    result_.value().SetBoolKey(GetAttachEncryptionSettingsPath(), true);
  }
}

UploadEncryptedReportingRequestBuilder::
    ~UploadEncryptedReportingRequestBuilder() = default;

UploadEncryptedReportingRequestBuilder&
UploadEncryptedReportingRequestBuilder::AddRecord(
    const EncryptedRecord& record) {
  if (!result_.has_value()) {
    // Some errors were already detected.
    return *this;
  }
  base::Value* const records_list =
      result_.value().FindListKey(GetEncryptedRecordListPath());
  if (!records_list || !records_list->is_list()) {
    NOTREACHED();  // Should not happen.
    return *this;
  }

  auto record_result = EncryptedRecordDictionaryBuilder(record).Build();
  if (!record_result.has_value()) {
    // Record has errors. Stop here.
    result_ = base::nullopt;
    return *this;
  }

  records_list->Append(std::move(record_result.value()));
  return *this;
}

base::Optional<base::Value> UploadEncryptedReportingRequestBuilder::Build() {
  return std::move(result_);
}

// static
base::StringPiece
UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath() {
  return kEncryptedRecordListKey;
}

// static
base::StringPiece
UploadEncryptedReportingRequestBuilder::GetAttachEncryptionSettingsPath() {
  return kAttachEncryptionSettingsKey;
}

EncryptedRecordDictionaryBuilder::EncryptedRecordDictionaryBuilder(
    const EncryptedRecord& record) {
  base::Value record_dictionary{base::Value::Type::DICTIONARY};

  // A record without sequencing information cannot be uploaded - deny it.
  if (!record.has_sequencing_information()) {
    return;
  }
  auto sequencing_information_result =
      SequencingInformationDictionaryBuilder(record.sequencing_information())
          .Build();
  if (!sequencing_information_result.has_value()) {
    // Sequencing information was improperly configured. Record cannot be
    // uploaded. Deny it.
    return;
  }
  record_dictionary.SetKey(GetSequencingInformationKeyPath(),
                           std::move(sequencing_information_result.value()));
  // For backwards compatibility, store unsigned sequencing information too.
  // The values are non-negative anyway, so the same builder can be used.
  auto unsigned_sequencing_information_result =
      SequencingInformationDictionaryBuilder(record.sequencing_information())
          .Build();
  if (!unsigned_sequencing_information_result.has_value()) {
    // Sequencing information was improperly configured. Record cannot be
    // uploaded. Deny it.
    return;
  }
  record_dictionary.SetKey(
      GetUnsignedSequencingInformationKeyPath(),
      std::move(unsigned_sequencing_information_result.value()));

  // Encryption information can be missing until we set up encryption as
  // mandatory.
  if (record.has_encryption_info()) {
    auto encryption_info_result =
        EncryptionInfoDictionaryBuilder(record.encryption_info()).Build();
    if (!encryption_info_result.has_value()) {
      // Encryption info has been corrupted or set improperly. Deny it.
      return;
    }
    record_dictionary.SetKey(GetEncryptionInfoPath(),
                             std::move(encryption_info_result.value()));
  }

  // Gap records won't fill in this field, so it can be missing.
  if (record.has_encrypted_wrapped_record()) {
    std::string base64_encode;
    base::Base64Encode(record.encrypted_wrapped_record(), &base64_encode);
    record_dictionary.SetStringKey(GetEncryptedWrappedRecordPath(),
                                   base64_encode);
  }

  // Result complete.
  result_ = std::move(record_dictionary);
}

EncryptedRecordDictionaryBuilder::~EncryptedRecordDictionaryBuilder() = default;

base::Optional<base::Value> EncryptedRecordDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
base::StringPiece
EncryptedRecordDictionaryBuilder::GetEncryptedWrappedRecordPath() {
  return kEncryptedWrappedRecord;
}

// static
base::StringPiece
EncryptedRecordDictionaryBuilder::GetUnsignedSequencingInformationKeyPath() {
  return kUnsignedSequencingInformationKey;
}

// static
base::StringPiece
EncryptedRecordDictionaryBuilder::GetSequencingInformationKeyPath() {
  return kSequencingInformationKey;
}

// static
base::StringPiece EncryptedRecordDictionaryBuilder::GetEncryptionInfoPath() {
  return kEncryptionInfoKey;
}

SequencingInformationDictionaryBuilder::SequencingInformationDictionaryBuilder(
    const SequencingInformation& sequencing_information) {
  // SequencingInformation requires all three fields be set.
  if (!sequencing_information.has_sequencing_id() ||
      !sequencing_information.has_generation_id() ||
      !sequencing_information.has_priority()) {
    return;
  }

  base::Value sequencing_dictionary{base::Value::Type::DICTIONARY};
  sequencing_dictionary.SetStringKey(
      GetSequencingIdPath(),
      base::NumberToString(sequencing_information.sequencing_id()));
  sequencing_dictionary.SetStringKey(
      GetGenerationIdPath(),
      base::NumberToString(sequencing_information.generation_id()));
  sequencing_dictionary.SetIntKey(GetPriorityPath(),
                                  sequencing_information.priority());
  result_ = std::move(sequencing_dictionary);
}

SequencingInformationDictionaryBuilder::
    ~SequencingInformationDictionaryBuilder() = default;

base::Optional<base::Value> SequencingInformationDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
base::StringPiece
SequencingInformationDictionaryBuilder::GetSequencingIdPath() {
  return kSequencingId;
}

// static
base::StringPiece
SequencingInformationDictionaryBuilder::GetGenerationIdPath() {
  return kGenerationId;
}

// static
base::StringPiece SequencingInformationDictionaryBuilder::GetPriorityPath() {
  return kPriority;
}

EncryptionInfoDictionaryBuilder::EncryptionInfoDictionaryBuilder(
    const EncryptionInfo& encryption_info) {
  base::Value encryption_info_dictionary{base::Value::Type::DICTIONARY};

  // EncryptionInfo requires both fields are set.
  if (!encryption_info.has_encryption_key() ||
      !encryption_info.has_public_key_id()) {
    return;
  }

  encryption_info_dictionary.SetStringKey(GetEncryptionKeyPath(),
                                          encryption_info.encryption_key());
  encryption_info_dictionary.SetStringKey(
      GetPublicKeyIdPath(),
      base::NumberToString(encryption_info.public_key_id()));
  result_ = std::move(encryption_info_dictionary);
}

EncryptionInfoDictionaryBuilder::~EncryptionInfoDictionaryBuilder() = default;

base::Optional<base::Value> EncryptionInfoDictionaryBuilder::Build() {
  return std::move(result_);
}

// static
base::StringPiece EncryptionInfoDictionaryBuilder::GetEncryptionKeyPath() {
  return kEncryptionKey;
}

// static
base::StringPiece EncryptionInfoDictionaryBuilder::GetPublicKeyIdPath() {
  return kPublicKeyId;
}

}  // namespace reporting
