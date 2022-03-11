// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TEST_UTIL_H_

#include <ostream>
#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

using ::testing::Matcher;

class RequestContainingRecordMatcher {
 public:
  using is_gtest_matcher = void;

  explicit RequestContainingRecordMatcher(
      base::StringPiece matched_record_json);
  bool MatchAndExplain(const base::Value::Dict& arg, std::ostream* os) const;
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

// Match a request Dict object that contains the given record
// |matched_record_json|. The match will be successful as long as any record in
// the request contains |matched_record_json| as a sub-dictionary -- they are
// not required to equal. In this way, you can specify only part of the record
// of interest (e.g., omit "encryptedWrappedRecord").
//
// This is templated because we expect the tested request comes in different
// forms, including their referenceness (gtest need the matcher type to also
// match references to some extent). As long as the type can be cast to a
// |base::Value::Dict| object, this matcher should work.
template <class T = base::Value::Dict>
Matcher<T> DoesRequestContainRecord(base::StringPiece matched_record_json) {
  return RequestContainingRecordMatcher(matched_record_json);
}

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TEST_UTIL_H_
