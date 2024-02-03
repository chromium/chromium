// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test support library for request payloads.

#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"

#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace reporting {

// Return true if s a properly formatted positive integer, i.e., is not empty,
// contains digits only and does not start with 0.
static bool IsPositiveInteger(std::string_view s) {
  if (s.empty()) {
    return false;
  }
  if (s.size() == 1) {
    return absl::ascii_isdigit(static_cast<unsigned char>(s[0]));
  }
  return s[0] != '0' && s.find_first_not_of("0123456789") == std::string::npos;
}

// Get the record list. If it can't, print the message to listener and return a
// null pointer.
static const base::Value::List* GetRecordList(const base::Value::Dict& arg,
                                              MatchResultListener* listener) {
  const auto* const record_list = arg.FindList(json_keys::kEncryptedRecordList);
  if (record_list == nullptr) {
    *listener << "No key named \"encryptedRecord\" in the argument or the "
                 "value is not a list.";
    return nullptr;
  }
  return record_list;
}

bool AttachEncryptionSettingsMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  const auto attach_encryption_settings =
      arg.FindBool(json_keys::kAttachEncryptionSettings);
  if (!attach_encryption_settings) {
    *listener << "No key named \"attachEncryptionSettings\" in the argument or "
                 "the value is not of bool type.";
    return false;
  }
  if (!attach_encryption_settings.value()) {
    *listener << "The value of \"attachEncryptionSettings\" is false.";
    return false;
  }
  return true;
}

void AttachEncryptionSettingsMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid attachEncryptionSettings field.";
}

void AttachEncryptionSettingsMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "has an invalid attachEncryptionSettings field.";
}

std::string AttachEncryptionSettingsMatcher::Name() const {
  return "attach-encryption-settings-matcher";
}

bool NoAttachEncryptionSettingsMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  if (arg.Find(json_keys::kAttachEncryptionSettings) != nullptr) {
    *listener << "Found \"attachEncryptionSettings\" in the argument.";
    return false;
  }
  return true;
}

void NoAttachEncryptionSettingsMatcher::DescribeTo(std::ostream* os) const {
  *os << "expectedly has no attachEncryptionSettings field.";
}

void NoAttachEncryptionSettingsMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "unexpectedly has an attachEncryptionSettings field.";
}

std::string NoAttachEncryptionSettingsMatcher::Name() const {
  return "no-attach-encryption-settings-matcher";
}

bool ConfigurationFileVersionMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  auto* attach_configuration_file =
      arg.Find(json_keys::kConfigurationFileVersion);
  if (!attach_configuration_file->GetIfInt().has_value()) {
    *listener << "No key named \"configurationFileVersion\" in the argument or "
                 "the value is not of int type.";
    return false;
  }
  return true;
}

void ConfigurationFileVersionMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid configurationFileVersion field.";
}

void ConfigurationFileVersionMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "has an invalid configurationFileVersion field.";
}

std::string ConfigurationFileVersionMatcher::Name() const {
  return "configuration-file-version-matcher";
}

bool NoConfigurationFileVersionMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  if (arg.Find(json_keys::kConfigurationFileVersion) != nullptr) {
    *listener << "Found \"configurationFileVersion\" in the argument.";
    return false;
  }
  return true;
}

void NoConfigurationFileVersionMatcher::DescribeTo(std::ostream* os) const {
  *os << "expectedly has no configurationFileVersion field.";
}

void NoConfigurationFileVersionMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "unexpectedly has an configurationFileVersion field.";
}

std::string NoConfigurationFileVersionMatcher::Name() const {
  return "no-configuration-file-version-matcher";
}

bool SourceMatcher::MatchAndExplain(const base::Value::Dict& arg,
                                    MatchResultListener* listener) const {
  if (arg.FindString(json_keys::kSource) == nullptr) {
    *listener << "No key named \"source\" or the value "
                 "is not a string in the argument.";
    return false;
  }
  return true;
}

void SourceMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid source field.";
}

void SourceMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "has an invalid source field.";
}

std::string SourceMatcher::Name() const {
  return "source-test-matcher";
}

bool NoSourceMatcher::MatchAndExplain(const base::Value::Dict& arg,
                                      MatchResultListener* listener) const {
  if (arg.Find(json_keys::kSource) != nullptr) {
    *listener << "Found \"source\" in the argument.";
    return false;
  }
  return true;
}

void NoSourceMatcher::DescribeTo(std::ostream* os) const {
  *os << "expectedly has no source field.";
}

void NoSourceMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "unexpectedly has an Source field.";
}

std::string NoSourceMatcher::Name() const {
  return "source-test-matcher";
}

void CompressionInformationMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid compression information field.";
}

void CompressionInformationMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "has an invalid compression information field.";
}

std::string CompressionInformationMatcher::Name() const {
  return "encrypted-record-matcher";
}

bool EncryptedRecordMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  const auto* const record_list = GetRecordList(arg, listener);
  return record_list != nullptr;
}

void EncryptedRecordMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid encryptedRecord field.";
}

void EncryptedRecordMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "has an invalid encryptedRecord field.";
}

std::string EncryptedRecordMatcher::Name() const {
  return "encrypted-record-matcher";
}

bool RequestIdMatcher::MatchAndExplain(const base::Value::Dict& arg,
                                       MatchResultListener* listener) const {
  const auto* const request_id = arg.FindString(json_keys::kRequestId);
  if (request_id == nullptr) {
    *listener << "No key named \"requestId\" in the argument or the value "
                 "is not a string.";
    return false;
  }
  if (request_id->empty()) {
    *listener << "Request ID is empty.";
    return false;
  }
  if (!base::ranges::all_of(*request_id, base::IsHexDigit<char>)) {
    *listener << "Request ID is not a hexadecimal number.";
    return false;
  }

  return true;
}

void RequestIdMatcher::DescribeTo(std::ostream* os) const {
  *os << "has a valid request ID.";
}

void RequestIdMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "has an invalid request ID.";
}

std::string RequestIdMatcher::Name() const {
  return "request-id-matcher";
}

bool RecordMatcher::MatchAndExplain(const base::Value::Dict& arg,
                                    MatchResultListener* listener) const {
  switch (mode_) {
    case Mode::FullRequest: {
      const auto* record_list = GetRecordList(arg, listener);
      if (record_list == nullptr) {
        return false;
      }

      for (const auto& record : *record_list) {
        const auto* record_dict = record.GetIfDict();
        if (record_dict == nullptr) {
          *listener << "Record " << record << " is not a dict.";
          return false;
        }
        if (!this->MatchAndExplainRecord(*record_dict, listener)) {
          return false;
        }
      }
      return true;
    }
    case Mode::RecordOnly: {
      return this->MatchAndExplainRecord(arg, listener);
    }
    default: {
      // Should never reach here.
      *listener << "Invalid mode of RecordMatcher encountered: "
                << static_cast<std::underlying_type_t<Mode>>(mode_);
      return false;
    }
  }
}

RecordMatcher& RecordMatcher::SetMode(RecordMatcher::Mode mode) {
  mode_ = mode;
  return *this;
}

bool CompressionInformationMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  const auto* const compression_info =
      record.FindDict(json_keys::kCompressionInformation);
  if (compression_info == nullptr) {
    *listener << "No key named \"compressionInformation\" or the value is "
                 "not a dict in record "
              << record << '.';
    return false;
  }

  const auto compression_algorithm =
      compression_info->FindInt(json_keys::kCompressionAlgorithm);
  if (!compression_algorithm.has_value()) {
    *listener << "No key named \"compressionAlgorithm\" under "
                 "compressionInformation "
                 "or the value is not an integer in record "
              << record << '.';
    return false;
  }
  if (compression_algorithm.value() > 1 || compression_algorithm < 0) {
    *listener << "The value of \"compressionInformation/compressionAlgorithm\" "
                 "is neither 0 nor 1 in record "
              << record << '.';
    return false;
  }

  return true;
}

bool EncryptedWrappedRecordRecordMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  if (record.FindString(json_keys::kEncryptedWrappedRecord) == nullptr) {
    *listener << "No key named \"encryptedWrappedRecord\" or the value "
                 "is not a string in record "
              << record << '.';
    return false;
  }
  return true;
}

void EncryptedWrappedRecordRecordMatcher::DescribeTo(std::ostream* os) const {
  *os << "has valid encrypted wrapped records.";
}

void EncryptedWrappedRecordRecordMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "has at least one invalid encrypted wrapped records or has missing "
         "encrypted wrapped records.";
}

std::string EncryptedWrappedRecordRecordMatcher::Name() const {
  return "encrypted-wrapped-record-record-matcher";
}

bool NoEncryptedWrappedRecordRecordMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  if (record.Find(json_keys::kEncryptedWrappedRecord) != nullptr) {
    *listener << "Found \"encryptedWrappedRecord\" in record " << record << '.';
    return false;
  }
  return true;
}

void NoEncryptedWrappedRecordRecordMatcher::DescribeTo(std::ostream* os) const {
  *os << "expectedly has no encrypted wrapped record.";
}

void NoEncryptedWrappedRecordRecordMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "has at least one encrypted wrapped record that it should not have.";
}

std::string NoEncryptedWrappedRecordRecordMatcher::Name() const {
  return "no-encrypted-wrapped-record-record-matcher";
}

bool SequenceInformationRecordMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  const auto* const sequence_information =
      record.FindDict(json_keys::kSequenceInformation);
  if (sequence_information == nullptr) {
    *listener << "No key named \"sequenceInformation\" or the value is "
                 "not a dict in record "
              << record << '.';
    return false;
  }

  if (!sequence_information->FindInt(json_keys::kPriority).has_value()) {
    *listener << "No key named \"sequenceInformation/priority\" or the "
                 "value is not an integer in record "
              << record << '.';
    return false;
  }

  for (const char* id : {json_keys::kSequencingId, json_keys::kGenerationId}) {
    const auto* const id_val = sequence_information->FindString(id);
    if (id_val == nullptr) {
      *listener << "No key named \"sequenceInformation/" << id
                << "\" or the value is not a string in record " << record
                << '.';
      return false;
    }
    if (!IsPositiveInteger(*id_val)) {
      *listener << "The value of \"sequenceInformation/" << id
                << "\" is not a properly formatted positive integer in record "
                << record << '.';
      return false;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (EncryptedReportingClient::GenerationGuidIsRequired()) {
    const auto* generation_guid =
        sequence_information->FindString(json_keys::kGenerationGuid);
    if ((!generation_guid || generation_guid->empty())) {
      *listener << "No key named \"sequenceInformation/generationGuid\" or the "
                   "value is not a string in record "
                << record << '.';
      return false;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return true;
}

void SequenceInformationRecordMatcher::DescribeTo(std::ostream* os) const {
  *os << "has valid sequence information.";
}

void SequenceInformationRecordMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "has invalid sequence information.";
}

std::string SequenceInformationRecordMatcher::Name() const {
  return "sequence-information-record-matcher";
}

bool RequestContainingRecordMatcher::IsSubDict(const base::Value::Dict& sub,
                                               const base::Value::Dict& super) {
  for (auto&& [key, sub_value] : sub) {
    const auto* super_value = super.Find(key);
    if (super_value == nullptr || *super_value != sub_value) {
      return false;
    }
  }
  return true;
}

RequestContainingRecordMatcher::RequestContainingRecordMatcher(
    std::string_view matched_record_json)
    : matched_record_json_(matched_record_json) {}

bool RequestContainingRecordMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  const auto* record_list = GetRecordList(arg, listener);
  if (record_list == nullptr) {
    return false;
  }

  const auto matched_record = base::JSONReader::Read(matched_record_json_);
  if (!matched_record.has_value()) {
    *listener << "The specified record cannot be parsed as a JSON object.";
    return false;
  }
  const auto* matched_record_dict = matched_record->GetIfDict();
  if (matched_record_dict == nullptr) {
    *listener
        << "The specified record must be a Dict itself because each record "
           "is a Dict.";
    return false;
  }

  for (const auto& record : *record_list) {
    const auto* record_dict = record.GetIfDict();
    if (!record_dict) {
      continue;
    }

    // Match each key and value of matched_record with those of the iterated
    // record_dict. In this way, users can specify part of a record instead
    // of the full record.
    if (IsSubDict(*matched_record_dict, *record_dict)) {
      return true;
    }
  }

  *listener << "The specified record is not found in the argument.";
  return false;
}

void RequestContainingRecordMatcher::DescribeTo(std::ostream* os) const {
  *os << "contains the specified record.";
}

void RequestContainingRecordMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "does not contain the specified record or there are other failed "
         "conditions.";
}

}  // namespace reporting
