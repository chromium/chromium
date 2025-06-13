// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/exception_processor.h"

#import <Foundation/Foundation.h>

#include "base/mac/os_crash_dumps.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExceptionProcessorTest : public testing::Test {
  void SetUp() override {
    base::mac::DisableOSCrashDumps();
    crash_reporter::InitializeCrashKeysForTesting();
    ResetObjcExceptionStateForTesting();
  }
};

struct CrashKeyValues {
  std::string firstexception;
  std::string firstexception_bt;
  std::string lastexception;
  std::string lastexception_bt;
};
CrashKeyValues GetExceptionCrashKeyValues() {
  return {crash_reporter::GetCrashKeyValue("firstexception"),
          crash_reporter::GetCrashKeyValue("firstexception_bt"),
          crash_reporter::GetCrashKeyValue("lastexception"),
          crash_reporter::GetCrashKeyValue("lastexception_bt")};
}

TEST(ExceptionProcessorTest, CrashKeysRecorded) {
  constexpr char kAtLeastEightHexValues[] = "(0x[[:xdigit:]]+ ){8}";

  CrashKeyValues initial_values = GetExceptionCrashKeyValues();
  EXPECT_THAT(initial_values.firstexception, testing::IsEmpty());
  EXPECT_THAT(initial_values.firstexception_bt, testing::IsEmpty());
  EXPECT_THAT(initial_values.lastexception, testing::IsEmpty());
  EXPECT_THAT(initial_values.lastexception_bt, testing::IsEmpty());

  @try {
    [NSException raise:@"ExceptionProcessorTest" format:@""];
  } @catch (id exception) {
  }

  CrashKeyValues after_first_exception = GetExceptionCrashKeyValues();
  EXPECT_THAT(after_first_exception.firstexception,
              testing::StartsWith("ExceptionProcessorTest reason"));
  EXPECT_THAT(after_first_exception.firstexception_bt,
              testing::ContainsRegex(kAtLeastEightHexValues));
  EXPECT_THAT(after_first_exception.lastexception, testing::IsEmpty());
  EXPECT_THAT(after_first_exception.lastexception_bt, testing::IsEmpty());

  @try {
    [NSException raise:@"ExceptionProcessorTest2" format:@""];
  } @catch (id exception) {
  }

  CrashKeyValues after_second_exception = GetExceptionCrashKeyValues();
  EXPECT_THAT(after_second_exception.firstexception,
              testing::StartsWith("ExceptionProcessorTest reason"));
  EXPECT_THAT(after_second_exception.firstexception_bt,
              testing::ContainsRegex(kAtLeastEightHexValues));
  EXPECT_THAT(after_second_exception.lastexception,
              testing::StartsWith("ExceptionProcessorTest2 reason"));
  EXPECT_THAT(after_second_exception.lastexception_bt,
              testing::ContainsRegex(kAtLeastEightHexValues));
}
