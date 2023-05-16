// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"

#include <sys/types.h>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kExampleUrl1[] = "https://example1.com/";

constexpr ino_t kInode1 = 1;
constexpr ino_t kInode2 = 2;

constexpr base::TimeDelta kCooldownTimeout = base::Seconds(5);

constexpr size_t kEntriesLimit = 100;

}  // namespace

class DlpFilesEventStorageTest : public testing::Test {
 public:
  DlpFilesEventStorageTest(const DlpFilesEventStorageTest&) = delete;
  DlpFilesEventStorageTest& operator=(const DlpFilesEventStorageTest&) = delete;

 protected:
  DlpFilesEventStorageTest() : profile_(std::make_unique<TestingProfile>()) {}
  ~DlpFilesEventStorageTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  const std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpFilesEventStorageTest, UpsertEvents) {
  DlpFilesEventStorage storage(kCooldownTimeout, kEntriesLimit);
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  storage.SetTaskRunnerForTesting(task_runner);

  const auto dst1 = DlpFileDestination(kExampleUrl1);
  const auto dst2 = DlpFileDestination(data_controls::Component::kDrive);

  // Insertion
  ASSERT_TRUE(storage.StoreEventAndCheckIfItShouldBeReported(kInode1, dst1));
  ASSERT_TRUE(storage.StoreEventAndCheckIfItShouldBeReported(kInode1, dst2));
  ASSERT_TRUE(storage.StoreEventAndCheckIfItShouldBeReported(kInode2, dst1));
  ASSERT_TRUE(storage.StoreEventAndCheckIfItShouldBeReported(kInode2, dst2));

  ASSERT_THAT(storage.GetSizeForTesting(), 4);

  task_runner->FastForwardBy(kCooldownTimeout / 2);

  // Update
  ASSERT_FALSE(storage.StoreEventAndCheckIfItShouldBeReported(kInode1, dst1));
  ASSERT_FALSE(storage.StoreEventAndCheckIfItShouldBeReported(kInode1, dst2));
  ASSERT_FALSE(storage.StoreEventAndCheckIfItShouldBeReported(kInode2, dst1));
  ASSERT_FALSE(storage.StoreEventAndCheckIfItShouldBeReported(kInode2, dst2));

  ASSERT_THAT(storage.GetSizeForTesting(), 4);

  // Automatic removal
  task_runner->FastForwardBy(kCooldownTimeout);
  ASSERT_THAT(storage.GetSizeForTesting(), 0);

  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kActiveFileEventsCount, 1, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kActiveFileEventsCount, 2, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kActiveFileEventsCount, 3, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kActiveFileEventsCount, 4, 5);
}

TEST_F(DlpFilesEventStorageTest, LimitEvents) {
  DlpFilesEventStorage storage(kCooldownTimeout, kEntriesLimit);
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  storage.SetTaskRunnerForTesting(task_runner);

  size_t count = 0;
  size_t max_inode = 11;
  size_t max_dst_index = 11;
  for (size_t inode = 0; inode < max_inode; ++inode) {
    for (size_t dst_index = 0; dst_index < max_dst_index; ++dst_index) {
      count++;
      auto dst = DlpFileDestination("https://example" +
                                    base::NumberToString(dst_index) + ".com/");
      if (count <= kEntriesLimit) {
        ASSERT_TRUE(storage.StoreEventAndCheckIfItShouldBeReported(inode, dst));
        ASSERT_THAT(storage.GetSizeForTesting(), count);
      } else {
        ASSERT_FALSE(
            storage.StoreEventAndCheckIfItShouldBeReported(inode, dst));
        ASSERT_THAT(storage.GetSizeForTesting(), kEntriesLimit);
      }
    }
  }
  for (size_t bucket = 1; bucket <= kEntriesLimit; ++bucket) {
    histogram_tester_.ExpectBucketCount(
        GetDlpHistogramPrefix() + dlp::kActiveFileEventsCount, bucket, 1);
  }
}

}  // namespace policy
