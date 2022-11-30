// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_histogram_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::AnyOf;

TEST(ReporterHistogramRecorder, NoSuffix) {
  registry_util::RegistryOverrideManager registry_manager;
  registry_manager.OverrideRegistry(HKEY_CURRENT_USER);

  base::HistogramTester histogram_tester;
  ReporterHistogramRecorder recorder("");

  // Test a simple histogram that doesn't access the registry.
  recorder.RecordVersion(base::Version("1.2.3"));
  histogram_tester.ExpectTotalCount("SoftwareReporter.MinorVersion", 1);
  histogram_tester.ExpectTotalCount("SoftwareReporter.MajorVersion", 1);

  // No error code found in registry.
  base::win::RegKey reg_key(HKEY_CURRENT_USER,
                            L"Software\\Google\\Software Removal Tool",
                            KEY_SET_VALUE | KEY_QUERY_VALUE);
  EXPECT_THAT(reg_key.DeleteValue(L"EngineErrorCode"),
              AnyOf(ERROR_SUCCESS, ERROR_FILE_NOT_FOUND));
  recorder.RecordEngineErrorCode();
  histogram_tester.ExpectTotalCount("SoftwareReporter.EngineErrorCode", 0);

  // Write an engine error to the registry, and ensure that the recorder logs it
  // and then clears it.
  constexpr DWORD kErrorCode = 0;
  EXPECT_EQ(reg_key.WriteValue(L"EngineErrorCode", kErrorCode), ERROR_SUCCESS);
  recorder.RecordEngineErrorCode();
  histogram_tester.ExpectTotalCount("SoftwareReporter.EngineErrorCode", 1);
  EXPECT_FALSE(reg_key.HasValue(L"EngineErrorCode"));
}

TEST(ReporterHistogramRecorder, WithSuffix) {
  registry_util::RegistryOverrideManager registry_manager;
  registry_manager.OverrideRegistry(HKEY_CURRENT_USER);

  base::HistogramTester histogram_tester;
  ReporterHistogramRecorder recorder("EngineSuffix");

  // Test a simple histogram that doesn't access the registry.
  recorder.RecordVersion(base::Version("1.2.3"));
  histogram_tester.ExpectTotalCount(
      "SoftwareReporter.MinorVersion_EngineSuffix", 1);
  histogram_tester.ExpectTotalCount(
      "SoftwareReporter.MajorVersion_EngineSuffix", 1);

  // No error code found in registry, at the suffix's subkey.
  base::win::RegKey reg_key(
      HKEY_CURRENT_USER,
      L"Software\\Google\\Software Removal Tool\\EngineSuffix",
      KEY_SET_VALUE | KEY_QUERY_VALUE);
  EXPECT_THAT(reg_key.DeleteValue(L"EngineErrorCode"),
              AnyOf(ERROR_SUCCESS, ERROR_FILE_NOT_FOUND));
  recorder.RecordEngineErrorCode();
  histogram_tester.ExpectTotalCount(
      "SoftwareReporter.EngineErrorCode_EngineSuffix", 0);

  // Write an engine error to the registry, at the suffix's subkey, and ensure
  // that the recorder logs it and then clears it.
  constexpr DWORD kErrorCode = 0;
  EXPECT_EQ(reg_key.WriteValue(L"EngineErrorCode", kErrorCode), ERROR_SUCCESS);
  recorder.RecordEngineErrorCode();
  histogram_tester.ExpectTotalCount(
      "SoftwareReporter.EngineErrorCode_EngineSuffix", 1);
  EXPECT_FALSE(reg_key.HasValue(L"EngineErrorCode"));
}

}  // namespace
}  // namespace safe_browsing
