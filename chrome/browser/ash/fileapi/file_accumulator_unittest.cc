// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_accumulator.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash {
namespace {

MATCHER(RecentFilesEq, "") {
  RecentFile a = std::get<0>(arg);
  RecentFile b = std::get<1>(arg);
  return a.url() == b.url() && a.last_modified() == b.last_modified();
}

RecentFile MakeRecentFile(const std::string& name,
                          const base::Time& last_modified) {
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey(), storage::kFileSystemTypeLocal, base::FilePath(name));
  return RecentFile(url, last_modified);
}

class FileAccumulatorTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(FileAccumulatorTest, AddGetClear) {
  RecentFile a_file = MakeRecentFile("a.jpg", base::Time::Now());
  RecentFile b_file = MakeRecentFile("b.jpg", base::Time::Now());

  FileAccumulator acc(10);
  EXPECT_TRUE(acc.Add(a_file));
  EXPECT_THAT(acc.Get(), testing::Pointwise(RecentFilesEq(), {a_file}));
  EXPECT_FALSE(acc.Add(b_file));
  acc.Clear();
  EXPECT_TRUE(acc.Add(b_file));
  EXPECT_THAT(acc.Get(), testing::Pointwise(RecentFilesEq(), {b_file}));
}

TEST_F(FileAccumulatorTest, CapacityAndOrder) {
  base::Time now = base::Time::Now();
  FileAccumulator acc(2);
  // File a.jpg is the most recently modified, then c.jpg, then b.jpg.
  EXPECT_TRUE(acc.Add(MakeRecentFile("a.jpg", now - base::Minutes(1))));
  EXPECT_TRUE(acc.Add(MakeRecentFile("b.jpg", now - base::Minutes(5))));
  EXPECT_TRUE(acc.Add(MakeRecentFile("c.jpg", now - base::Minutes(2))));
  std::vector<RecentFile> content_1 = acc.Get();
  // Expect a.jpg followed by c.jpg, based on last modified time.
  EXPECT_EQ(content_1.size(), 2u);
  EXPECT_EQ(content_1[0].url().path().value(), "a.jpg");
  EXPECT_EQ(content_1[1].url().path().value(), "c.jpg");

  EXPECT_FALSE(acc.Add(MakeRecentFile("d.jpg", now - base::Minutes(3))));
  std::vector<RecentFile> content_2 = acc.Get();
  EXPECT_THAT(content_2, testing::Pointwise(RecentFilesEq(), content_1));
}

}  // namespace
}  // namespace ash
