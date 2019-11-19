// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_result.h"

#include <stddef.h>

#include "base/logging.h"

namespace base {

TestResultPart::TestResultPart() = default;
TestResultPart::~TestResultPart() = default;

TestResultPart::TestResultPart(const TestResultPart& other) = default;
TestResultPart::TestResultPart(TestResultPart&& other) = default;
TestResultPart& TestResultPart::operator=(const TestResultPart& other) =
    default;
TestResultPart& TestResultPart::operator=(TestResultPart&& other) = default;

// static
bool TestResultPart::TypeFromString(const std::string& str, Type* type) {
  if (str == "success")
    *type = kSuccess;
  else if (str == "failure")
    *type = kNonFatalFailure;
  else if (str == "fatal_failure")
    *type = kFatalFailure;
  else if (str == "skip")
    *type = kSkip;
  else
    return false;
  return true;
}

std::string TestResultPart::TypeAsString() const {
  switch (type) {
    case kSuccess:
      return "success";
    case kNonFatalFailure:
      return "failure";
    case kFatalFailure:
      return "fatal_failure";
    case kSkip:
      return "skip";
    default:
      NOTREACHED();
  }
  return "unknown";
}

TestResult::TestResult() : status(TEST_UNKNOWN) {
}

TestResult::~TestResult() = default;

TestResult::TestResult(const TestResult& other) = default;
TestResult::TestResult(TestResult&& other) = default;
TestResult& TestResult::operator=(const TestResult& other) = default;
TestResult& TestResult::operator=(TestResult&& other) = default;

std::string TestResult::StatusAsString() const {
  switch (status) {
    case TEST_UNKNOWN:
      return "UNKNOWN";
    case TEST_SUCCESS:
      return "SUCCESS";
    case TEST_FAILURE:
      return "FAILURE";
    case TEST_FAILURE_ON_EXIT:
      return "FAILURE_ON_EXIT";
    case TEST_CRASH:
      return "CRASH";
    case TEST_TIMEOUT:
      return "TIMEOUT";
    case TEST_SKIPPED:
      return "SKIPPED";
    case TEST_EXCESSIVE_OUTPUT:
      return "EXCESSIVE_OUTPUT";
    case TEST_NOT_RUN:
      return "NOTRUN";
      // Rely on compiler warnings to ensure all possible values are handled.
  }

  NOTREACHED();
  return std::string();
}

std::string TestResult::GetTestName() const {
  size_t dot_pos = full_name.find('.');
  CHECK_NE(dot_pos, std::string::npos);
  return full_name.substr(dot_pos + 1);
}

std::string TestResult::GetTestCaseName() const {
  size_t dot_pos = full_name.find('.');
  CHECK_NE(dot_pos, std::string::npos);
  return full_name.substr(0, dot_pos);
}

}  // namespace base
