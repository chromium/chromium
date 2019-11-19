// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

constexpr char kMetricLibrarySizeBytes[] = "library_size";
constexpr char kMetricTimeToLoadLibraryMs[] = "time_to_load_library";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter("", story);
  reporter.RegisterImportantMetric(kMetricLibrarySizeBytes, "bytes");
  reporter.RegisterImportantMetric(kMetricTimeToLoadLibraryMs, "ms");
  return reporter;
}

// Measures the size (bytes) and time to load (sec) of a native library.
// |library_relative_dir| is the relative path based on DIR_MODULE.
void MeasureSizeAndTimeToLoadNativeLibrary(
    const base::FilePath& library_relative_dir,
    const base::FilePath& library_name) {
  base::FilePath output_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &output_dir));
  output_dir = output_dir.Append(library_relative_dir);
  base::FilePath library_path = output_dir.Append(library_name);
  ASSERT_TRUE(base::PathExists(library_path)) << library_path.value();

  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(library_path, &size));
  auto reporter = SetUpReporter(library_name.AsUTF8Unsafe());
  reporter.AddResult(kMetricLibrarySizeBytes, static_cast<size_t>(size));

  base::NativeLibraryLoadError error;
  base::TimeTicks start = base::TimeTicks::Now();
  base::NativeLibrary native_library =
      base::LoadNativeLibrary(library_path, &error);
  double delta = (base::TimeTicks::Now() - start).InMillisecondsF();
  ASSERT_TRUE(native_library) << "Error loading library: " << error.ToString();
  base::UnloadNativeLibrary(native_library);
  reporter.AddResult(kMetricTimeToLoadLibraryMs, delta);
}

void MeasureSizeAndTimeToLoadCdm(const std::string& cdm_base_dir,
                                 const std::string& cdm_name) {
  MeasureSizeAndTimeToLoadNativeLibrary(
      media::GetPlatformSpecificDirectory(cdm_base_dir),
      base::FilePath::FromUTF8Unsafe(cdm_name));
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_WIDEVINE)
TEST(LoadCDMPerfTest, Widevine) {
  MeasureSizeAndTimeToLoadCdm(
      kWidevineCdmBaseDirectory,
      base::GetNativeLibraryName(kWidevineCdmLibraryName));
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

TEST(LoadCDMPerfTest, ExternalClearKey) {
  MeasureSizeAndTimeToLoadCdm(
      media::kClearKeyCdmBaseDirectory,
      base::GetLoadableModuleName(media::kClearKeyCdmLibraryName));
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
