// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_H_

#include <list>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

using ::testing::AllOfArray;
using ::testing::Matcher;
using ::testing::MatchResultListener;

// Matcher interface for each request validity matcher. A request validity
// matcher is a matcher that verifies one aspect of the general validity of a
// request JSON object.
class RequestValidityMatcherInterface {
 public:
  using is_gtest_matcher = void;
  virtual ~RequestValidityMatcherInterface() = default;
  virtual bool MatchAndExplain(const base::Value::Dict& arg,
                               MatchResultListener* listener) const = 0;
  virtual void DescribeTo(std::ostream* os) const = 0;
  virtual void DescribeNegationTo(std::ostream* os) const = 0;
  // Name of this matcher.
  virtual std::string Name() const = 0;
};

// encryptedRecord must be a list. This matcher is recommended to be applies
// before verifying the details of any record (e.g., via |RecordMatcher|) to
// generate more readable error messages.
class EncryptedRecordMatcher : public RequestValidityMatcherInterface {
 public:
  bool MatchAndExplain(const base::Value::Dict& arg,
                       MatchResultListener* listener) const override;
  void DescribeTo(std::ostream* os) const override;
  void DescribeNegationTo(std::ostream* os) const override;
  std::string Name() const override;
};

// requestId must be a hexadecimal number represented as a string.
class RequestIdMatcher : public RequestValidityMatcherInterface {
 public:
  bool MatchAndExplain(const base::Value::Dict& arg,
                       MatchResultListener* listener) const override;
  void DescribeTo(std::ostream* os) const override;
  void DescribeNegationTo(std::ostream* os) const override;
  std::string Name() const override;
};

// Base class of all matchers that verify one aspect of a record.
class RecordMatcher : public RequestValidityMatcherInterface {
 public:
  bool MatchAndExplain(const base::Value::Dict& arg,
                       MatchResultListener* listener) const final;
  // Match and explain the given record.
  virtual bool MatchAndExplainRecord(const base::Value::Dict& arg,
                                     MatchResultListener* listener) const = 0;
};

// Verify the encryptedWrappedRecord field of each record.
class EncryptedWrappedRecordRecordMatcher : public RecordMatcher {
 public:
  bool MatchAndExplainRecord(const base::Value::Dict& arg,
                             MatchResultListener* listener) const override;
  void DescribeTo(std::ostream* os) const override;
  void DescribeNegationTo(std::ostream* os) const override;
  std::string Name() const override;
};

// Verify the sequenceInformation field of each record.
class SequenceInformationRecordMatcher : public RecordMatcher {
 public:
  bool MatchAndExplainRecord(const base::Value::Dict& arg,
                             MatchResultListener* listener) const override;
  void DescribeTo(std::ostream* os) const override;
  void DescribeNegationTo(std::ostream* os) const override;
  std::string Name() const override;
};

// Build a matcher that can be used to verify the general validity of a
// request matcher while accommodating the variety of requirements (e.g.,
// some verification must be loosen because the request is intentionally
// malformed for a particular test case). To use this class, call
// CreateDataUpload() or CreateEmpty() to create an instance. Adapt matchers by
// calling AppendMatcher() or RemoveMatcher().
template <class T = base::Value::Dict>
class RequestValidityMatcherBuilder {
 public:
  // We can't support copy because after copying, matcher_list_t's iterators in
  // matcher_index_ would point to the elements in the wrong
  // RequestValidityMatcherBuilder instance.
  RequestValidityMatcherBuilder(const RequestValidityMatcherBuilder<T>&) =
      delete;
  RequestValidityMatcherBuilder<T>& operator=(
      const RequestValidityMatcherBuilder<T>&) = delete;
  RequestValidityMatcherBuilder(RequestValidityMatcherBuilder<T>&&) = default;
  RequestValidityMatcherBuilder<T>& operator=(
      RequestValidityMatcherBuilder<T>&&) = default;

  // Creates and returns a |RequestValidityMatcherBuilder| instance that
  // contains no matchers.
  static RequestValidityMatcherBuilder<T> CreateEmpty() {
    return RequestValidityMatcherBuilder<T>();
  }

  // Creates and returns a |RequestValidityMatcherBuilder| instance that
  // contains a matcher that is suited for verifying a data upload request.
  static RequestValidityMatcherBuilder<T> CreateDataUpload() {
    // We need to call std::move here because AppendMatcher returns an lvalue
    // reference. It's important to move the object here because the iterators
    // in matcher_index_ would lose validity otherwise. (If a std::list object
    // is moved, its existing iterators remain valid and point to the elements
    // in this new object. But if it is copied and the old object is destroyed,
    // the existing iterators become invalid.)
    return std::move(RequestValidityMatcherBuilder<T>::CreateEmpty()
                         .AppendMatcher(RequestIdMatcher())
                         .AppendMatcher(EncryptedRecordMatcher())
                         .AppendMatcher(EncryptedWrappedRecordRecordMatcher())
                         .AppendMatcher(SequenceInformationRecordMatcher()));
  }

  // Builds and returns the |Matcher<T>| object.
  [[nodiscard]] Matcher<T> Build() const { return AllOfArray(matchers_); }

  // Append a matcher. This object will take over the ownership of matcher and
  // matcher must be allocated on heap (e.g., by using the "new" operator).
  template <class RequestValidityMatcher>
  RequestValidityMatcherBuilder<T>& AppendMatcher(
      const RequestValidityMatcher& matcher) {
    static_assert(std::is_base_of<RequestValidityMatcherInterface,
                                  RequestValidityMatcher>::value,
                  "matcher must be of type RequestValidityMatcherInterface.");
    matchers_.emplace_back(matcher);
    matcher_index_.emplace(matcher.Name(), std::prev(matchers_.cend()));
    return *this;
  }

  // Remove a matcher.
  RequestValidityMatcherBuilder<T>& RemoveMatcher(base::StringPiece name) {
    auto matcher_it = matcher_index_.find(name);
    EXPECT_NE(matcher_it, matcher_index_.end())
        << "Matcher \"" << name << "\" not found.";
    matchers_.erase(matcher_it->second);
    matcher_index_.erase(matcher_it);
    return *this;
  }

 private:
  // List of matchers.
  using matcher_list_t = std::list<Matcher<T>>;
  matcher_list_t matchers_{};
  // A name to matcher mapping.
  base::flat_map<std::string, typename matcher_list_t::const_iterator>
      matcher_index_{};

  RequestValidityMatcherBuilder() = default;
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
// To enable or skip some part of the validity checks (e.g., because your test
// case intentionally creates a malformed request), instead of using this
// wrapper, you must call
//
//     RequestValidityMatcherBuilder<>::CreateDataUpload()
//            .AppendMatcher(...)
//            .RemoveMatcher(...)
//            ...
//            .Build()
template <class T = base::Value::Dict>
Matcher<T> IsDataUploadRequestValid() {
  return RequestValidityMatcherBuilder<T>::CreateDataUpload().Build();
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
