// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/test.h"

#include <string>

#include "base/json/json_reader.h"

namespace reporting {

// Return true if s a properly formatted positive integer, i.e., is not empty,
// contains digits only and does not start with 0.
static bool IsPositiveInteger(base::StringPiece s) {
  if (s.empty()) {
    return false;
  } else if (s.size() == 1) {
    return std::isdigit(s[0]);
  } else {
    return s[0] != '0' &&
           s.find_first_not_of("0123456789") == std::string::npos;
  }
}

// Get the record list. If it can't, print the message to listener and return a
// null pointer.
static const base::Value::List* GetRecordList(const base::Value::Dict& arg,
                                              MatchResultListener* listener) {
  const auto* const record_list = arg.FindList("encryptedRecord");
  if (record_list == nullptr) {
    *listener << "No key named \"encryptedRecord\" in the argument or the "
                 "value is not a list.";
    return nullptr;
  }
  return record_list;
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
  const auto* const request_id = arg.FindString("requestId");
  if (request_id == nullptr) {
    *listener << "No key named \"requestId\" in the argument or the value "
                 "is not a string.";
    return false;
  }
  if (request_id->empty()) {
    *listener << "Request ID is empty.";
    return false;
  }
  if (request_id->find_first_not_of("0123456789abcdefABCDEF") !=
      std::string::npos) {
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

bool EncryptedWrappedRecordRecordMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  if (record.FindString("encryptedWrappedRecord") == nullptr) {
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
  *os << "has at least one invalid encrypted wrapped records.";
}

std::string EncryptedWrappedRecordRecordMatcher::Name() const {
  return "encrypted-wrapped-record-record-matcher";
}

bool SequenceInformationRecordMatcher::MatchAndExplainRecord(
    const base::Value::Dict& record,
    MatchResultListener* listener) const {
  const auto* const sequence_information =
      record.FindDict("sequenceInformation");
  if (sequence_information == nullptr) {
    *listener << "No key named \"sequenceInformation\" or the value is "
                 "not a dict in record "
              << record << '.';
    return false;
  }

  if (!sequence_information->FindInt("priority").has_value()) {
    *listener << "No key named \"sequenceInformation/priority\" or the "
                 "value is not an integer in record "
              << record << '.';
    return false;
  }

  for (const char* id : {"sequencingId", "generationId"}) {
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
    base::StringPiece matched_record_json)
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
