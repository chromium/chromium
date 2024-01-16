// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pmf_utils.h"

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android {

TEST(PmfUtilsTest, CalculatePrivateMemoryFootprint) {
  const char kStatusFile[] =
      "First:    1\n"
      "Second:  2 kB\n"
      "VmSwap: 10 kB\n"
      "Third:  10 kB\n"
      "VmHWM:  72 kB\n"
      "Last:     8";
  const char kStatmFile[] = "100 40 25 0 0";
  const uint64_t expected_swap_kb = 10;
  const uint64_t expected_pmf =
      (40 - 25) * getpagesize() / 1024 + expected_swap_kb;

  base::FilePath statm_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&statm_path));
  EXPECT_TRUE(base::WriteFile(statm_path, kStatmFile));
  base::FilePath status_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&status_path));
  EXPECT_TRUE(base::WriteFile(status_path, kStatusFile));

  base::File statm_file(statm_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File status_file(status_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  std::optional<uint64_t> pmf =
      PmfUtils::CalculatePrivateMemoryFootprintForTesting(statm_file,
                                                          status_file);

  EXPECT_TRUE(pmf.has_value());
  EXPECT_EQ(expected_pmf, pmf.value() / 1024);
}

}  // namespace base::android
