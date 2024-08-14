// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/dlp/test/files_policy_notification_manager_test_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42");
}

storage::FileSystemURL CreateFileSystemURL(const blink::StorageKey key,
                                           const std::string& path) {
  return storage::FileSystemURL::CreateForTest(
      key, storage::kFileSystemTypeLocal, base::FilePath::FromUTF8Unsafe(path));
}

file_manager::io_task::IOTaskController* GetIOTaskController(Profile* profile) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  CHECK(volume_manager);
  CHECK(volume_manager->io_task_controller());
  return volume_manager->io_task_controller();
}

base::FilePath AddCopyOrMoveIOTask(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    file_manager::io_task::IOTaskId id,
    file_manager::io_task::OperationType type,
    const base::FilePath& dir,
    const std::string& file,
    const blink::StorageKey key) {
  CHECK(type == file_manager::io_task::OperationType::kCopy ||
        type == file_manager::io_task::OperationType::kMove);

  base::FilePath src_file_path = dir.AppendASCII(file);
  if (!CreateDummyFile(src_file_path)) {
    return base::FilePath();
  }
  auto src_url = CreateFileSystemURL(key, src_file_path.value());
  if (!src_url.is_valid()) {
    return base::FilePath();
  }
  auto dst_url = CreateFileSystemURL(key, dir.value());

  GetIOTaskController(profile)->Add(
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          type, std::vector<storage::FileSystemURL>({src_url}), dst_url,
          profile, file_system_context));

  return src_file_path;
}

void VerifyFilesWarningUMAs(const base::HistogramTester& histogram_tester,
                            std::vector<base::Bucket> action_warned_buckets,
                            std::vector<base::Bucket> warning_count_buckets,
                            std::vector<base::Bucket> action_timedout_buckets) {
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionWarnedUMA)),
              testing::ElementsAreArray(action_warned_buckets.data(),
                                        action_warned_buckets.size()));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(data_controls::GetDlpHistogramPrefix() +
                                     data_controls::dlp::kFilesWarnedCountUMA),
      testing::ElementsAreArray(warning_count_buckets.data(),
                                warning_count_buckets.size()));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  data_controls::dlp::kFileActionWarnTimedOutUMA),
              testing::ElementsAreArray(action_timedout_buckets.data(),
                                        action_timedout_buckets.size()));
}

}  // namespace policy
