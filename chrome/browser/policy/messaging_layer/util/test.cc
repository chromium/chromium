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

DataUploadRequestValidityMatcher::Settings&
DataUploadRequestValidityMatcher::Settings::SetCheckRecordDetails(bool flag) {
  check_record_details_ = flag;
  return *this;
}

DataUploadRequestValidityMatcher::Settings&
DataUploadRequestValidityMatcher::Settings::SetCheckEncryptedRecord(bool flag) {
  check_encrypted_record_ = flag;
  return *this;
}

bool DataUploadRequestValidityMatcher::CheckRecord(
    const base::Value& record,
    MatchResultListener* listener) const {
  const auto* record_dict = record.GetIfDict();
  if (record_dict == nullptr) {
    *listener << "Record " << record << " is not a dict.";
    return false;
  }

  if (record_dict->FindString("encryptedWrappedRecord") == nullptr) {
    *listener << "No key named \"encryptedWrappedRecord\" or the value "
                 "is not a string in record "
              << record << '.';
    return false;
  }

  {  // sequence information
    const auto* sequence_information =
        record_dict->FindDict("sequenceInformation");
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
      const auto* id_val = sequence_information->FindString(id);
      if (id_val == nullptr) {
        *listener << "No key named \"sequenceInformation/" << id
                  << "\" or the value is not a string in record " << record
                  << '.';
        return false;
      }
      if (!IsPositiveInteger(*id_val)) {
        *listener
            << "The value of \"sequenceInformation/" << id
            << "\" is not a properly formatted positive integer in record "
            << record << '.';
        return false;
      }
    }
  }

  return true;
}

DataUploadRequestValidityMatcher::DataUploadRequestValidityMatcher(
    const Settings& settings)
    : settings_(settings) {}

bool DataUploadRequestValidityMatcher::MatchAndExplain(
    const base::Value::Dict& arg,
    MatchResultListener* listener) const {
  if (settings_.check_encrypted_record_) {
    const auto* record_list = arg.FindList("encryptedRecord");
    if (record_list == nullptr) {
      *listener
          << "No key named \"encryptedRecord\" in the argument or the value is "
             "not a list.";
      return false;
    }

    // examine each record
    if (settings_.check_record_details_) {
      for (const auto& record : *record_list) {
        if (!CheckRecord(record, listener)) {
          return false;
        }
      }
    }
  }

  const auto* request_id = arg.FindString("requestId");
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

void DataUploadRequestValidityMatcher::DescribeTo(std::ostream* os) const {
  *os << "is valid.";
}

void DataUploadRequestValidityMatcher::DescribeNegationTo(
    std::ostream* os) const {
  *os << "is invalid.";
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
  const auto* record_list = arg.FindList("encryptedRecord");
  if (record_list == nullptr) {
    *listener << "No key named \"encryptedRecord\" in the argument or the "
                 "value is not a list.";
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
