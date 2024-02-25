// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_TEST_FILES_POLICY_NOTIFICATION_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_TEST_FILES_POLICY_NOTIFICATION_MANAGER_TEST_UTILS_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// The id of the first notification FPNM shows.
constexpr char kNotificationId[] = "dlp_files_0";

// Creates a dummy file in `path`. Returns whether it was created successfully.
bool CreateDummyFile(const base::FilePath& path);

// Creates and returns a file system URL based on `path`.
storage::FileSystemURL CreateFileSystemURL(const blink::StorageKey,
                                           const std::string& path);

// Returns the IOTaskController for `profile`.
file_manager::io_task::IOTaskController* GetIOTaskController(Profile* profile);

// Creates and adds a CopyOrMoveIOTask with `task_id`, `type` (must be copy or
// move), destination set to `dir` and source set to `dir/file`. Returns the
// path of the source file.
base::FilePath AddCopyOrMoveIOTask(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    file_manager::io_task::IOTaskId id,
    file_manager::io_task::OperationType type,
    const base::FilePath& dir,
    const std::string& file,
    const blink::StorageKey key);

// Verifies warning UMAs against the expected buckets passed as parameters.
void VerifyFilesWarningUMAs(const base::HistogramTester& histogram_tester,
                            std::vector<base::Bucket> action_warned_buckets,
                            std::vector<base::Bucket> warning_count_buckets,
                            std::vector<base::Bucket> action_timedout_buckets);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_TEST_FILES_POLICY_NOTIFICATION_MANAGER_TEST_UTILS_H_
