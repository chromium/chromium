// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
namespace i18n {

namespace {

// Directory path to the tzdata configuration files, used in tests only.
const char kTestTzDataDirPath[] = "/pkg/base/test/data/tzdata/icu/44/le";

// File path to the text file containing the expected ICU library revision, for
// example "2019c". This file is available in production.
const char kTZDataRevisionFilePath[] = "/config/tzdata/icu/revision.txt";

}  // namespace

class TimeZoneDataTest : public testing::Test {
 protected:
  void TearDown() override {
    ResetGlobalsForTesting();

    // ICU must be set back up in case e.g. a log statement that formats times
    // uses it.
    test::InitializeICUForTesting();
  }

  void GetActualRevision(std::string* icu_version) {
    UErrorCode err = U_ZERO_ERROR;
    *icu_version = icu::TimeZone::getTZDataVersion(err);
    ASSERT_EQ(U_ZERO_ERROR, err) << u_errorName(err);
  }
};

// Loads a file revision.txt from the actual underlying filesystem, which
// contains the tzdata version we expect to be able to load.  It then attempts
// to load this configuration from the default path and compares the version it
// obtained from the load with the expected version, failing on version
// mismatch.
TEST_F(TimeZoneDataTest, CompareSystemRevisionWithExpected) {
  ASSERT_TRUE(base::PathExists(base::FilePath(kTZDataRevisionFilePath)));
  ResetGlobalsForTesting();

  ASSERT_TRUE(InitializeICU());
  std::string expected;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath(kTZDataRevisionFilePath),
                                     &expected))
      << "Could not read from path: " << kTZDataRevisionFilePath;
  std::string actual;
  GetActualRevision(&actual);
  EXPECT_EQ(expected, actual);
}

// Verifies that the current version of the ICU library in use can load ICU
// data in a specific version format (in this case 44).  Designed to fail if
// the ICU library version used in Chromium drifts from version 44 so much that
// the library is no longer able to load the old tzdata.  If the test fails,
// this could be a sign that all platforms Chromium runs on need to upgrade the
// ICU library versions.
TEST_F(TimeZoneDataTest, TestLoadingTimeZoneDataFromKnownConfigs) {
  ASSERT_TRUE(base::DirectoryExists(base::FilePath(kTestTzDataDirPath)));
  ResetGlobalsForTesting();
  SetIcuTimeZoneDataDirForTesting(kTestTzDataDirPath);

  ASSERT_TRUE(InitializeICU());
  std::string actual;
  GetActualRevision(&actual);
  EXPECT_EQ("2019a", actual) << "If ICU no longer supports this tzdata "
                                "version, tzdata version needs to be upgraded";
}

using TimeZoneDataDeathTest = TimeZoneDataTest;

TEST_F(TimeZoneDataDeathTest, CrashesWithNonexistentPath) {
  ResetGlobalsForTesting();
  SetIcuTimeZoneDataDirForTesting("/some/nonexistent/path");

  EXPECT_DEATH(InitializeICU(),
               "Could not open directory: '/some/nonexistent/path'");
}

}  // namespace i18n
}  // namespace base
