// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/test_util.h"

#include "base/json/json_reader.h"

namespace reporting {

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
    std::ostream* os) const {
  const auto* record_list = arg.FindList("encryptedRecord");
  if (record_list == nullptr) {
    *os << "No key named \"encryptedRecord\" in the argument.";
    return false;
  }

  const auto matched_record = base::JSONReader::Read(matched_record_json_);
  if (!matched_record.has_value()) {
    *os << "The specified record cannot be parsed as a JSON object.";
    return false;
  }
  const auto* matched_record_dict = matched_record->GetIfDict();
  if (matched_record_dict == nullptr) {
    *os << "The specified record must be a Dict itself because each record "
           "is a Dict.";
    return false;
  }

  for (const auto& record : *record_list) {
    const auto* record_dict = record.GetIfDict();
    if (!record_dict) {
      continue;
    }

    // Match each key and value of matched_record with those of the iterated
    // record_dict. In this way, users can specify part of a record instead of
    // the full record.
    if (IsSubDict(*matched_record_dict, *record_dict)) {
      return true;
    }
  }

  *os << "The specified record is not found in the argument.";
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
