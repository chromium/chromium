// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/user_data_stats_recorder.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace user_data_stats_recorder {

namespace {
constexpr char kCodeCachePath[] = "Code Cache";
constexpr char kCodeCacheUMAName[] = "CodeCache";
constexpr char kTextFileContent[] = "Hello, World!";
constexpr int kTextFileSize = sizeof(kTextFileContent);
}  // namespace

TEST(UserDataStatsRecorderTest, ComputeDirectorySizeWithoutLinks) {
  base::ScopedTempDir dir_1;
  ASSERT_TRUE(dir_1.CreateUniqueTempDir());

  ASSERT_TRUE(
      base::WriteFile(dir_1.GetPath().Append(FILE_PATH_LITERAL("file1")),
                      kTextFileContent, kTextFileSize));
  ASSERT_TRUE(
      base::CreateDirectory(dir_1.GetPath().Append(FILE_PATH_LITERAL("dir"))));
  ASSERT_TRUE(base::WriteFile(dir_1.GetPath()
                                  .Append(FILE_PATH_LITERAL("dir"))
                                  .Append(FILE_PATH_LITERAL("file2")),
                              kTextFileContent, kTextFileSize));

  // Check that `ComputeDirectorySizeWithoutLinks` returns the sum of sizes of
  // the two files in the directory.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            kTextFileSize * 2);

  base::ScopedTempDir dir_2;
  ASSERT_TRUE(dir_2.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")),
                      kTextFileContent, kTextFileSize));

  ASSERT_TRUE(CreateSymbolicLink(
      dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")),
      dir_1.GetPath().Append(FILE_PATH_LITERAL("link_to_file3"))));

  // Check that `ComputeDirectorySizeWithoutLinks` does not follow symlinks from
  // `dir_1` to `dir_2`.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            kTextFileSize * 2);
}

TEST(UserDataStatsRecorderTest, GetUMAItemName) {
  base::FilePath profile_data_dir("/home/chronos/user");

  EXPECT_STREQ(GetUMAItemName(profile_data_dir.Append(kCodeCachePath)).c_str(),
               kCodeCacheUMAName);

  EXPECT_STREQ(
      GetUMAItemName(profile_data_dir.Append(FILE_PATH_LITERAL("abcd")))
          .c_str(),
      kUnknownUMAName);
}

TEST(UserDataStatsRecorderTest, RecordUserDataSize) {
  base::HistogramTester histogram_tester;

  base::FilePath profile_data_dir("/home/chronos/user");
  // Size in bytes.
  int64_t size = 4 * 1024 * 1024;
  RecordUserDataSize(profile_data_dir.Append(kCodeCachePath), size);

  std::string uma_name =
      std::string(kUserDataStatsRecorderDataSize) + kCodeCacheUMAName;

  histogram_tester.ExpectTotalCount(uma_name, 1);
  histogram_tester.ExpectBucketCount(uma_name, size / 1024 / 1024, 1);
}

TEST(UserDataStatsRecorderTest, RecordUserDataSizes) {
  base::HistogramTester histogram_tester;
  base::ScopedTempDir profile_data_dir;
  ASSERT_TRUE(profile_data_dir.CreateUniqueTempDir());

  ASSERT_TRUE(
      base::CreateDirectory(profile_data_dir.GetPath().Append(kCodeCachePath)));
  ASSERT_TRUE(base::WriteFile(profile_data_dir.GetPath()
                                  .Append(kCodeCachePath)
                                  .Append(FILE_PATH_LITERAL("cache")),
                              kTextFileContent, kTextFileSize));
  ASSERT_TRUE(base::WriteFile(
      profile_data_dir.GetPath().Append(FILE_PATH_LITERAL("abcd")),
      kTextFileContent, kTextFileSize));
  std::string uma_name_1 =
      std::string(kUserDataStatsRecorderDataSize) + kCodeCacheUMAName;
  std::string uma_name_3 =
      std::string(kUserDataStatsRecorderDataSize) + kUnknownUMAName;

  RecordUserDataSizes(profile_data_dir.GetPath());

  histogram_tester.ExpectTotalCount(uma_name_1, 1);
  histogram_tester.ExpectTotalCount(uma_name_3, 1);

  histogram_tester.ExpectBucketCount(uma_name_1, kTextFileSize / 1024 / 1024,
                                     1);
  histogram_tester.ExpectBucketCount(uma_name_3, kTextFileSize / 1024 / 1024,
                                     1);
}

}  // namespace user_data_stats_recorder
}  // namespace crosapi
