// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace extensions {
namespace {

void WriteFileWithChecks(const base::FilePath full_file_path) {
  ASSERT_TRUE(WriteFile(full_file_path, full_file_path.MaybeAsASCII()));
}

std::vector<std::pair<base::FilePath, bool>> SetUpTestFiles(
    const base::ScopedTempDir& temp_dir,
    std::vector<base::FilePath>& files) {
  std::vector<std::pair<base::FilePath, bool>> created_files;
  for (const base::FilePath& path : files) {
    const base::FilePath full_file_path = temp_dir.GetPath().Append(path);
    WriteFileWithChecks(full_file_path);
    created_files.emplace_back(full_file_path, false);
  }
  return created_files;
}

}  // namespace

TEST(SearchByPatternTest, CreateFnmatchQuery) {
  ASSERT_EQ(CreateFnmatchQuery(""), "*");
  ASSERT_EQ(CreateFnmatchQuery("1"), "*1*");
  ASSERT_EQ(CreateFnmatchQuery("\""), "*\"*");
  ASSERT_EQ(CreateFnmatchQuery("a"), "*[aA]*");
  ASSERT_EQ(CreateFnmatchQuery("abc"), "*[aA][bB][cC]*");
  ASSERT_EQ(CreateFnmatchQuery("a*c"), "*[aA]*[cC]*");
}

TEST(SearchByPatternTest, SearchByPattern) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<base::FilePath> test_files = {
      base::FilePath::FromASCII("aaa"),
  };
  auto created_files = SetUpTestFiles(temp_dir, test_files);

  base::Time min_modified_time;
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 1970 00:00 UTC", &min_modified_time));

  auto found_with_a =
      SearchByPattern(temp_dir.GetPath(), "a", min_modified_time,
                      ash::RecentSource::FileType::kAll, 1);
  EXPECT_THAT(found_with_a, ElementsAre(created_files[0]));

  auto found_with_b =
      SearchByPattern(temp_dir.GetPath(), "b", min_modified_time,
                      ash::RecentSource::FileType::kAll, 1);
  EXPECT_THAT(found_with_b, ElementsAre());

  auto found_with_empty =
      SearchByPattern(temp_dir.GetPath(), "", min_modified_time,
                      ash::RecentSource::FileType::kAll, 1);
  EXPECT_THAT(found_with_empty, ElementsAre(created_files[0]));

  auto found_with_asterisk =
      SearchByPattern(temp_dir.GetPath(), "", min_modified_time,
                      ash::RecentSource::FileType::kAll, 1);
  EXPECT_THAT(found_with_asterisk, ElementsAre(created_files[0]));
}

}  // namespace extensions
