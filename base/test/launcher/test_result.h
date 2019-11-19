// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_RESULT_H_
#define BASE_TEST_LAUNCHER_TEST_RESULT_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace base {

// Structure contains result of a single EXPECT/ASSERT/SUCCESS/SKIP.
struct TestResultPart {
  enum Type {
    kSuccess,          // SUCCESS
    kNonFatalFailure,  // EXPECT
    kFatalFailure,     // ASSERT
    kSkip,             // SKIP
  };
  Type type;

  TestResultPart();
  ~TestResultPart();

  TestResultPart(const TestResultPart& other);
  TestResultPart(TestResultPart&& other);
  TestResultPart& operator=(const TestResultPart& other);
  TestResultPart& operator=(TestResultPart&& other);

  // Convert type to string and back.
  static bool TypeFromString(const std::string& str, Type* type);
  std::string TypeAsString() const;

  // Filename and line of EXPECT/ASSERT.
  std::string file_name;
  int line_number;

  // Message without stacktrace, etc.
  std::string summary;

  // Complete message.
  std::string message;
};

// Structure containing result of a single test.
struct TestResult {
  enum Status {
    TEST_UNKNOWN,           // Status not set.
    TEST_SUCCESS,           // Test passed.
    TEST_FAILURE,           // Assertion failure (e.g. EXPECT_TRUE, not DCHECK).
    TEST_FAILURE_ON_EXIT,   // Passed but executable exit code was non-zero.
    TEST_TIMEOUT,           // Test timed out and was killed.
    TEST_CRASH,             // Test crashed (includes CHECK/DCHECK failures).
    TEST_SKIPPED,           // Test skipped (not run at all).
    TEST_EXCESSIVE_OUTPUT,  // Test exceeded output limit.
    TEST_NOT_RUN,           // Test has not yet been run.
  };

  TestResult();
  ~TestResult();

  TestResult(const TestResult& other);
  TestResult(TestResult&& other);
  TestResult& operator=(const TestResult& other);
  TestResult& operator=(TestResult&& other);

  // Returns the test status as string (e.g. for display).
  std::string StatusAsString() const;

  // Returns the test name (e.g. "B" for "A.B").
  std::string GetTestName() const;

  // Returns the test case name (e.g. "A" for "A.B").
  std::string GetTestCaseName() const;

  // Returns true if the test has completed (i.e. the test binary exited
  // normally, possibly with an exit code indicating failure, but didn't crash
  // or time out in the middle of the test).
  bool completed() const {
    return status == TEST_SUCCESS ||
        status == TEST_FAILURE ||
        status == TEST_FAILURE_ON_EXIT ||
        status == TEST_EXCESSIVE_OUTPUT;
  }

  // Full name of the test (e.g. "A.B").
  std::string full_name;

  Status status;

  // Time it took to run the test.
  base::TimeDelta elapsed_time;

  // Output of just this test (optional).
  std::string output_snippet;

  // Information about failed expectations.
  std::vector<TestResultPart> test_result_parts;
};

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_RESULT_H_
