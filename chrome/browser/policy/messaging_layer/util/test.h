// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_H_

#include <ostream>
#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

using ::testing::Matcher;
using ::testing::MatchResultListener;

class DataUploadRequestValidityMatcher {
 public:
  using is_gtest_matcher = void;

  class Settings {
   public:
    Settings() = default;
    Settings(const Settings& other) = default;
    Settings(Settings&& other) = default;
    // Enable or disable checking the existence of key "encryptedRecord" and
    // that the value is a list. If disabled, implies the effect of
    // SetCheckRecordDetails(false).
    Settings& SetCheckEncryptedRecord(bool flag);
    // Enable or disable checking the details of each record. Useful to disable
    // if the test case includes intentionally malformed records.
    Settings& SetCheckRecordDetails(bool flag);

   private:
    friend DataUploadRequestValidityMatcher;
    bool check_encrypted_record_ = true;
    bool check_record_details_ = true;
  };

  DataUploadRequestValidityMatcher() = default;
  explicit DataUploadRequestValidityMatcher(const Settings& settings);
  bool MatchAndExplain(const base::Value::Dict& arg,
                       MatchResultListener* listener) const;
  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  const Settings settings_{};
  // Check the validity of the specified record. Return true if the record is
  // valid.
  bool CheckRecord(const base::Value& record,
                   MatchResultListener* listener) const;
};

class RequestContainingRecordMatcher {
 public:
  using is_gtest_matcher = void;

  explicit RequestContainingRecordMatcher(
      base::StringPiece matched_record_json);
  bool MatchAndExplain(const base::Value::Dict& arg,
                       MatchResultListener* os) const;
  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  const std::string matched_record_json_;

  // Determine if |sub| is a sub-dictionary of |super|. That means, whether
  // |super| contains all keys of |sub| and the values corresponding to each of
  // |sub|'s keys equal. This method does not call itself recursively on values
  // that are dictionaries.
  static bool IsSubDict(const base::Value::Dict& sub,
                        const base::Value::Dict& super);
};

// The following matcher functions templated because we expect the tested
// request comes in different forms, including their referenceness (gtest need
// the matcher type to also match references to some extent). As long as the
// type can be cast to a |base::Value::Dict| object, this matcher should work.

// Match a data upload request that is valid. This matcher is intended to be
// called for most tested data upload requests to verify whether the request is
// valid on some basic fronts, such as containing an "encryptedRecord" key, etc.
//
// You can use settings to enable or skip some part of the validity checks if
// your test case intentionally creates a malformed request.
template <class T = base::Value::Dict>
Matcher<T> IsDataUploadRequestValid(
    const DataUploadRequestValidityMatcher::Settings& settings) {
  return DataUploadRequestValidityMatcher(settings);
}
template <class T = base::Value::Dict>
Matcher<T> IsDataUploadRequestValid() {
  return DataUploadRequestValidityMatcher();
}

// Match a request that contains the given record |matched_record_json|. The
// match will be successful as long as any record in the request contains
// |matched_record_json| as a sub-dictionary -- they are not required to equal.
// In this way, you can specify only part of the record of interest (e.g., omit
// "encryptedWrappedRecord").
template <class T = base::Value::Dict>
Matcher<T> DoesRequestContainRecord(base::StringPiece matched_record_json) {
  return RequestContainingRecordMatcher(matched_record_json);
}

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_H_
